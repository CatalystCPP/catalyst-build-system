#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#include "catalyst/hooks.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/fetch.hpp"

namespace catalyst::fetch {
namespace fs = std::filesystem;

namespace {

std::expected<void, std::string> fetchVcpkg(const std::string &name) {
    catalyst::logger.log(LogLevel::DEBUG, "Fetching vcpkg dependency: {}", name);
    char *vcpkg_root_env = std::getenv("VCPKG_ROOT");
    if (vcpkg_root_env == nullptr) {
        catalyst::logger.log(LogLevel::ERROR, "VCPKG_ROOT environment variable not set.");
        return std::unexpected(
            "VCPKG_ROOT environment variable not set. Please set it to your vcpkg installation directory.");
    }
    fs::path vcpkg_root(vcpkg_root_env);
    fs::path vcpkg_exe = vcpkg_root / "vcpkg";
#if defined(_WIN32)
    vcpkg_exe.replace_extension(".exe");
#endif
    std::string command = std::format("\"{}\" install {}", vcpkg_exe.string(), name);
    catalyst::logger.log(LogLevel::DEBUG, "Executing command: {}", command);
    catalyst::logger.log(LogLevel::DEBUG, "Fetching: {} from vcpkg", name);
    if (catalyst::processExec({vcpkg_exe.string(), "install", name}).value().get() != 0) {
        catalyst::logger.log(LogLevel::ERROR, "Failed to fetch dependency: {}", name);
        return std::unexpected(std::format("Failed to fetch dependency: {}", name));
    }
    return {};
}

std::expected<void, std::string>
fetchGit(std::string build_dir, std::string name, std::string source, std::string version, std::string hash = "") {
    catalyst::logger.log(LogLevel::DEBUG, "Fetching git dependency: {}@{} (hash: {}) from {}", name, version, hash, source);
    fs::path dep_path = fs::path(build_dir) / "catalyst-libs" / name;
    
    std::string target = hash.empty() ? version : hash;
    std::println(std::cout, "Fetching: {}@{} from {}", name, target, source);
    
    std::vector<std::string> args;
    if (hash.empty()) {
        args = {"git", "clone", "--depth", "1"};
        if (version == "latest") {
            args.push_back(source);
            args.push_back(dep_path.string());
        } else {
            args.push_back("--branch");
            args.push_back(version);
            args.push_back(source);
            args.push_back(dep_path.string());
        }
        if (catalyst::processExec(std::move(args)).value().get() != 0) {
            catalyst::logger.log(LogLevel::ERROR, "Failed to fetch dependency: {}", name);
            return std::unexpected(std::format("Failed to fetch dependency: {}", name));
        }
    } else {
        // If we have a hash, we clone and then checkout the hash.
        // We can still try --depth 1 if we're lucky, but it's safer to clone and checkout.
        // Actually, let's try to be efficient:
        args = {"git", "clone", source, dep_path.string()};
        if (catalyst::processExec(std::move(args)).value().get() != 0) {
            catalyst::logger.log(LogLevel::ERROR, "Failed to clone dependency: {}", name);
            return std::unexpected(std::format("Failed to clone dependency: {}", name));
        }
        
        args = {"git", "-C", dep_path.string(), "checkout", hash};
        if (catalyst::processExec(std::move(args)).value().get() != 0) {
            catalyst::logger.log(LogLevel::ERROR, "Failed to checkout hash {} for dependency: {}", hash, name);
            return std::unexpected(std::format("Failed to checkout hash {} for dependency: {}", hash, name));
        }
    }
    
    return {};
}

std::expected<void, std::string> fetchSystem(const std::string &name) {
    // assuming installed on system
    catalyst::logger.log(LogLevel::DEBUG, "Skipping fetch for system dependency: {}", name);
    return {};
}

std::expected<void, std::string>
fetchLocal(const std::string &name, const std::string &path, const std::vector<std::string> &profiles) {
    fs::path local_path = fs::absolute(path);
    std::string visited_env = std::getenv("CATALYST_VISITED") ? std::getenv("CATALYST_VISITED") : "";

    // Cycle detection
    std::stringstream ss(visited_env);
    std::string segment;
    while (std::getline(ss, segment, ':')) {
        if (!segment.empty() && fs::equivalent(fs::path(segment), local_path)) {
            return std::unexpected(std::format("Dependency cycle detected involving {}", local_path.string()));
        }
    }

    std::string new_visited = visited_env.empty() ? local_path.string() : visited_env + ":" + local_path.string();

    catalyst::logger.log(LogLevel::DEBUG, "Recursively building local dependency: {} at {}", name, local_path.string());
    std::println(std::cout, "Building local dependency: {} at {}", name, local_path.string());

    std::vector<std::string> args = {"catalyst", "build"};
    if (profiles.size() != 0) {
        args.push_back("--profiles");
        for (const auto &p : profiles) {
            args.push_back(p);
        }
    }

    std::unordered_map<std::string, std::string> env_map;
    env_map["CATALYST_VISITED"] = new_visited;

    auto res = catalyst::processExec(std::move(args), local_path.string(), env_map);
    if (!res) {
        return std::unexpected(res.error());
    }

    if (res.value().get() != 0) {
        return std::unexpected(std::format("Failed to build local dependency: {}", name));
    }

    return {};
}

struct LockedDep {
    std::string hash;
    std::string url;
    std::string version;
    std::string triplet;
    std::string path;
};

} // namespace

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.log(LogLevel::DEBUG, "Fetch subcommand invoked.");
    catalyst::logger.log(LogLevel::DEBUG, "Composing profiles.");
    utils::yaml::Configuration config{parse_args.profiles};

    catalyst::logger.log(LogLevel::DEBUG, "Running pre-fetch hooks.");
    if (auto res = hooks::preFetch(config); !res) {
        catalyst::logger.log(LogLevel::ERROR, "Pre-fetch hook failed: {}", res.error());
        return res;
    }

    // Load lockfile if it exists
    std::unordered_map<std::string, LockedDep> lockfile_deps;
    fs::path lockfile_path = "catalyst.lock";
    if (parse_args.workspace) {
        lockfile_path = parse_args.workspace->getRoot() / "catalyst.lock";
    }

    if (fs::exists(lockfile_path)) {
        catalyst::logger.log(LogLevel::INFO, "Using lockfile at {}", lockfile_path.string());
        try {
            YAML::Node lock_node = YAML::LoadFile(lockfile_path.string());
            if (lock_node["dependencies"] && lock_node["dependencies"].IsSequence()) {
                for (const auto &dep : lock_node["dependencies"]) {
                    if (dep["name"]) {
                        LockedDep ld;
                        if (dep["hash"]) ld.hash = dep["hash"].as<std::string>();
                        if (dep["url"]) ld.url = dep["url"].as<std::string>();
                        if (dep["version"]) ld.version = dep["version"].as<std::string>();
                        if (dep["triplet"]) ld.triplet = dep["triplet"].as<std::string>();
                        if (dep["path"]) ld.path = dep["path"].as<std::string>();
                        lockfile_deps[dep["name"].as<std::string>()] = ld;
                    }
                }
            }
        } catch (const std::exception &e) {
            catalyst::logger.log(LogLevel::WARN, "Failed to load lockfile: {}", e.what());
        }
    }

    std::string build_dir = config.getString("manifest.dirs.build").value_or("build");
    if (auto deps = config.getRoot()["dependencies"]; deps && deps.IsSequence()) {
        for (int ii = 0; auto dep : deps) {
            if (!dep["name"]) {
                catalyst::logger.log(LogLevel::ERROR, "Dependency: {} does not define field: name", ii);
                return std::unexpected(std::format("Dependency: {} does not define field: name", ii));
            }
            if (!dep["source"]) {
                catalyst::logger.log(
                    LogLevel::ERROR, "Dependency: {} does not define field: source", dep["name"].as<std::string>());
                return std::unexpected(
                    std::format("Dependency: {} does not define field: source", dep["name"].as<std::string>()));
            }
            auto name = dep["name"].as<std::string>();
            auto source = dep["source"].as<std::string>();

            if (parse_args.workspace) {
                if (auto member = parse_args.workspace->findPackage(name)) {
                    catalyst::logger.log(LogLevel::INFO,
                                         "Dependency '{}' found in workspace at '{}'. Linking...",
                                         name,
                                         member->path.string());
                    fs::path lib_path = fs::path(build_dir) / "catalyst-libs" / name;

                    try {
                        if (fs::exists(lib_path) || fs::is_symlink(lib_path)) {
                            if (fs::is_symlink(lib_path)) {
                                if (fs::read_symlink(lib_path) != member->path) {
                                    fs::remove(lib_path);
                                    fs::create_directory_symlink(member->path, lib_path);
                                }
                            } else {
                                fs::remove_all(lib_path);
                                fs::create_directory_symlink(member->path, lib_path);
                            }
                        } else {
                            fs::create_directories(lib_path.parent_path());
                            fs::create_directory_symlink(member->path, lib_path);
                        }
                    } catch (const std::exception &e) {
                        catalyst::logger.log(LogLevel::ERROR, "Failed to link workspace dependency: {}", e.what());
                        return std::unexpected(e.what());
                    }
                    continue;
                }
            }

            catalyst::logger.log(LogLevel::DEBUG, "Fetching dependency '{}' from '{}'", name, source);
            
            // Check if locked
            std::string locked_hash = "";
            std::string locked_url = "";
            std::string locked_version = "";
            std::string locked_triplet = "";
            std::string locked_path = "";
            if (lockfile_deps.contains(name)) {
                locked_hash = lockfile_deps[name].hash;
                locked_url = lockfile_deps[name].url;
                locked_version = lockfile_deps[name].version;
                locked_triplet = lockfile_deps[name].triplet;
                locked_path = lockfile_deps[name].path;
                catalyst::logger.log(LogLevel::DEBUG, "Dependency '{}' is locked.", name);
            }

            if (source == "vcpkg") {
                auto version = !locked_version.empty() ? locked_version : (dep["version"] ? dep["version"].as<std::string>() : "");
                auto triplet = !locked_triplet.empty() ? locked_triplet : (dep["triplet"] ? dep["triplet"].as<std::string>() : "");
                
                if (version.empty()) {
                    return std::unexpected(std::format("vcpkg dependency '{}' is missing version.", name));
                }
                if (triplet.empty()) {
                    return std::unexpected(std::format("vcpkg dependency '{}' is missing triplet.", name));
                }
                if (auto res = fetchVcpkg(name); !res)
                    return std::unexpected(res.error());
            } else if (source == "system") {
                if (auto res = fetchSystem(name); !res)
                    return std::unexpected(res.error());
            } else if (source == "local") {
                auto path = !locked_path.empty() ? locked_path : (dep["path"] ? dep["path"].as<std::string>() : "");
                if (path.empty()) {
                    return std::unexpected(std::format("Local dependency '{}' is missing path.", name));
                }
                std::vector<std::string> profiles_vec;
                if (dep["profiles"] && dep["profiles"].IsSequence()) {
                    profiles_vec = dep["profiles"].as<std::vector<std::string>>();
                }
                if (auto res = fetchLocal(name, path, profiles_vec); !res)
                    return std::unexpected(res.error());
            } else {
                fs::path dep_path = fs::path(build_dir) / "catalyst-libs" / name;
                if (fs::exists(dep_path)) {
                    std::println(std::cout, "Skipping fetch for existing git dependency: {}", name);
                } else {
                    auto version = !locked_version.empty() ? locked_version : (dep["version"] ? dep["version"].as<std::string>() : "latest");
                    std::string url = !locked_url.empty() ? locked_url : ((source == "git" && dep["url"]) ? dep["url"].as<std::string>() : source);
                    if (auto res = fetchGit(build_dir, name, url, version, locked_hash); !res)
                        return std::unexpected(res.error());
                }
            }
            ++ii;
        }
    }

    catalyst::logger.log(LogLevel::DEBUG, "Running post-fetch hooks.");
    if (auto res = hooks::postFetch(config); !res) {
        catalyst::logger.log(LogLevel::ERROR, "Post-fetch hook failed: {}", res.error());
        return res;
    }

    catalyst::logger.log(LogLevel::DEBUG, "Fetch subcommand finished successfully.");
    return {};
}
} // namespace catalyst::fetch
