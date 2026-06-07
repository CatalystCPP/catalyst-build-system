#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <future>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/fetch.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::fetch {
namespace fs = std::filesystem;

namespace {

std::expected<void, std::string> fetchVcpkg(const std::string &name, const std::string &triplet) {
    catalyst::logger.debug("Fetching vcpkg dependency: {} with triplet: {}", name, triplet);
    char *vcpkg_root_env = std::getenv("VCPKG_ROOT");
    if (vcpkg_root_env == nullptr) {
        return std::unexpected(
            "VCPKG_ROOT environment variable not set. Please set it to your vcpkg installation directory.");
    }
    fs::path vcpkg_root(vcpkg_root_env);
    fs::path vcpkg_exe = vcpkg_root / "vcpkg";
#if defined(_WIN32)
    vcpkg_exe.replace_extension(".exe");
#endif
    std::string target = name + ":" + triplet;
    std::string command = std::format("\"{}\" install {}", vcpkg_exe.string(), target);
    catalyst::logger.debug("Executing command: {}", command);
    catalyst::logger.debug("Fetching: {} from vcpkg", target);
    if (catalyst::processExec({vcpkg_exe.string(), "install", target}).value().get() != 0) {
        return std::unexpected(std::format("Failed to fetch dependency: {}", target));
    }
    return {};
}

std::expected<void, std::string> fetchGit(
    const std::string &build_dir, std::string name, std::string source, std::string version, std::string hash = "") {
    catalyst::logger.debug("Fetching git dependency: {}@{} (hash: {}) from {}", name, version, hash, source);
    fs::path dep_path = fs::path(build_dir) / "catalyst-libs" / name;

    std::string target = hash.empty() ? version : hash;
    std::println(std::cout, "Fetching: {}@{} from {}", name, target, source);

    std::vector<std::string> args;
    if (hash.empty()) {
        args = {"git", "clone", "--depth", "1"};
        if (version == "latest") {
            args.emplace_back("--");
            args.push_back(source);
            args.push_back(dep_path.string());
        } else {
            args.emplace_back("--branch");
            args.push_back(version);
            args.emplace_back("--");
            args.push_back(source);
            args.push_back(dep_path.string());
        }
        if (catalyst::processExec(std::move(args)).value().get() != 0) {
            return std::unexpected(std::format("Failed to fetch dependency: {}", name));
        }
    } else {
        // If we have a hash, we clone and then checkout the hash.
        // We can still try --depth 1 if we're lucky, but it's safer to clone and checkout.
        // Actually, let's try to be efficient:
        args = {"git", "clone", source, dep_path.string()};
        if (catalyst::processExec(std::move(args)).value().get() != 0) {
            return std::unexpected(std::format("Failed to clone dependency: {}", name));
        }

        args = {"git", "-C", dep_path.string(), "checkout", hash};
        if (catalyst::processExec(std::move(args)).value().get() != 0) {
            return std::unexpected(std::format("Failed to checkout hash {} for dependency: {}", hash, name));
        }
    }

    return {};
}

std::expected<void, std::string> fetchSystem(const std::string &name) {
    // assuming installed on system
    catalyst::logger.debug("Skipping fetch for system dependency: {}", name);
    return {};
}

struct FetchLocalArgs {
    std::string name;
    std::string path;
    std::vector<std::string> profiles;
};

std::expected<void, std::string> fetchLocal(const FetchLocalArgs &fn_args) {
    const std::string &name = fn_args.name;
    const std::string &path = fn_args.path;
    const std::vector<std::string> &profiles = fn_args.profiles;
    std::error_code ec;
    fs::path local_path = fs::weakly_canonical(path, ec);
    if (ec) {
        return std::unexpected(
            std::format("Failed to resolve path '{}' for local dependency '{}': {}", path, name, ec.message()));
    }
    std::string local_path_str = local_path.string();
    const char *visited_env_tmp = std::getenv("CATALYST_VISITED");
    std::string visited_env = visited_env_tmp ? visited_env_tmp : "";

    // Cycle detection
    std::string_view visited_view(visited_env);
    size_t start = 0;
    while (start < visited_view.length()) {
        size_t end = visited_view.find(':', start);
        if (end == std::string_view::npos)
            end = visited_view.length();
        std::string_view segment = visited_view.substr(start, end - start);
        if (!segment.empty()) {
            std::error_code ec;
            if (fs::equivalent(fs::path(segment), local_path, ec) || (!ec && segment == local_path_str)) {
                return std::unexpected(std::format("Dependency cycle detected involving {}", local_path_str));
            }
        }
        start = end + 1;
    }

    std::string new_visited = visited_env.empty() ? local_path_str : visited_env + ":" + local_path_str;

    catalyst::logger.debug("Recursively building local dependency: {} at {}", name, local_path.string());
    std::println(std::cout, "Building local dependency: {} at {}", name, local_path.string());

    std::vector<std::string> args = {"catalyst", "build"};
    if (profiles.size() != 0) {
        args.emplace_back("--profiles");
        for (const auto &p : profiles) {
            args.push_back(p);
        }
    }

    std::unordered_map<std::string, std::string> env_map;
    env_map["CATALYST_VISITED"] = new_visited;
    env_map["CATALYST_MACHINE"] = "1";
    if (catalyst::logger.getVerboseLogging())
        env_map["CATALYST_VERBOSE"] = "1";

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

std::expected<void, std::string> fetchDependency(const YAML::Node &dep,
                                                 const std::string &build_dir,
                                                 const std::unordered_map<std::string, LockedDep> &lockfile_deps,
                                                 const Parse &parse_args) {

    auto name = dep["name"].as<std::string>();
    auto source = dep["source"].as<std::string>();

    if (parse_args.workspace) {
        if (auto member = parse_args.workspace->findPackage(name)) {
            catalyst::logger.info(
                "Dependency '{}' found in workspace at '{}'. Linking...", name, member->path.string());
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
                return std::unexpected(e.what());
            }
            return {};
        }
    }

    catalyst::logger.debug("Fetching dependency '{}' from '{}'", name, source);

    // Check if locked
    std::string locked_hash;
    std::string locked_url;
    std::string locked_version;
    std::string locked_triplet;
    std::string locked_path;
    if (lockfile_deps.contains(name)) {
        locked_hash = lockfile_deps.at(name).hash;
        locked_url = lockfile_deps.at(name).url;
        locked_version = lockfile_deps.at(name).version;
        locked_triplet = lockfile_deps.at(name).triplet;
        locked_path = lockfile_deps.at(name).path;
        catalyst::logger.debug("Dependency '{}' is locked.", name);
    }

    if (source == "vcpkg") {
        std::string version;
        if (!locked_version.empty())
            version = locked_version;
        else if (dep["version"])
            version = dep["version"].as<std::string>();
        else
            return std::unexpected(std::format("vcpkg dependency '{}' is missing version.", name));

        std::string triplet;
        if (!locked_triplet.empty())
            triplet = locked_triplet;
        else if (dep["triplet"])
            triplet = dep["triplet"].as<std::string>();
        else
            return std::unexpected(std::format("vcpkg dependency '{}' is missing triplet.", name));

        if (auto res = fetchVcpkg(name, triplet); !res)
            return std::unexpected(res.error());

    } else if (source == "system") {
        if (auto res = fetchSystem(name); !res)
            return std::unexpected(res.error());
    } else if (source == "local") {
        std::string path;
        if (!locked_path.empty())
            path = locked_path;
        else if (dep["path"])
            path = dep["path"].as<std::string>();
        else
            return std::unexpected(std::format("Local dependency '{}' is missing path.", name));

        std::vector<std::string> profiles_vec;
        if (dep["profiles"] && dep["profiles"].IsSequence()) {
            profiles_vec = dep["profiles"].as<std::vector<std::string>>();
        }
        if (auto res = fetchLocal({.name = name, .path = path, .profiles = profiles_vec}); !res)
            return std::unexpected(res.error());
    } else {
        fs::path dep_path = fs::path(build_dir) / "catalyst-libs" / name;
        if (fs::exists(dep_path)) {
            std::println(std::cout, "Skipping fetch for existing git dependency: {}", name);
        } else {
            std::string version;
            if (!locked_version.empty())
                version = locked_version;
            else if (dep["version"])
                version = dep["version"].as<std::string>();
            else
                version = "latest";

            std::string url;
            if (!locked_url.empty())
                url = locked_url;
            else if (source == "git" && dep["url"])
                url = dep["url"].as<std::string>();
            else
                url = source;

            if (auto res = fetchGit(build_dir, name, url, version, locked_hash); !res)
                return std::unexpected(res.error());
        }
    }
    return {};
}

} // namespace

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.debug("Fetch subcommand invoked.");
    catalyst::logger.debug("Composing profiles.");
    utils::yaml::Configuration config{parse_args.profiles};

    catalyst::logger.debug("Running pre-fetch hooks.");
    if (auto res = hooks::preFetch(config); !res) {
        return res;
    }

    // Load lockfile if it exists
    std::unordered_map<std::string, LockedDep> lockfile_deps;
    fs::path lockfile_path = "catalyst.lock";
    if (parse_args.workspace) {
        lockfile_path = parse_args.workspace->getRoot() / "catalyst.lock";
    }

    if (fs::exists(lockfile_path)) {
        catalyst::logger.info("Using lockfile at {}", lockfile_path.string());
        try {
            YAML::Node lock_node = YAML::LoadFile(lockfile_path.string());
            if (lock_node["dependencies"] && lock_node["dependencies"].IsSequence()) {
                for (const auto &dep : lock_node["dependencies"]) {
                    if (dep["name"]) {
                        LockedDep ld;
                        if (dep["hash"])
                            ld.hash = dep["hash"].as<std::string>();
                        if (dep["url"])
                            ld.url = dep["url"].as<std::string>();
                        if (dep["version"])
                            ld.version = dep["version"].as<std::string>();
                        if (dep["triplet"])
                            ld.triplet = dep["triplet"].as<std::string>();
                        if (dep["path"])
                            ld.path = dep["path"].as<std::string>();
                        lockfile_deps[dep["name"].as<std::string>()] = ld;
                    }
                }
            }
        } catch (const std::exception &e) {
            catalyst::logger.warn("Failed to load lockfile: {}", e.what());
        }
    }

    std::string build_dir = config.getBuildDir().string();
    if (auto deps = config.getRoot()["dependencies"]; deps && deps.IsSequence()) {
        std::vector<YAML::Node> parallel_deps;
        std::vector<YAML::Node> serial_deps;
        std::unordered_set<std::string> seen_deps;

        for (int ii = 0; auto dep : deps) {
            if (!dep["name"]) {
                return std::unexpected(std::format("Dependency: {} does not define field: name", ii));
            }
            if (!dep["source"]) {
                catalyst::logger.error("Dependency: {} does not define field: source", dep["name"].as<std::string>());
                return std::unexpected(
                    std::format("Dependency: {} does not define field: source", dep["name"].as<std::string>()));
            }

            std::string name = dep["name"].as<std::string>();

            // 1. Deduplicate by logical target name
            if (seen_deps.contains(name)) {
                catalyst::logger.debug("Skipping duplicate dependency entry: {}", name);
                ++ii;
                continue;
            }
            seen_deps.insert(name);

            std::string source = dep["source"].as<std::string>();
            bool is_workspace_member = parse_args.workspace && parse_args.workspace->findPackage(name).has_value();

            // 2. Categorize by safety
            // Workspace links, vcpkg installs, and local builds mutate shared state
            if (is_workspace_member || source == "vcpkg" || source == "local") {
                serial_deps.push_back(dep);
            } else {
                parallel_deps.push_back(dep);
            }
            ++ii;
        }

        // 3. Execute parallel-safe (git, system)
        if (!parallel_deps.empty()) {
            std::vector<std::future<std::expected<void, std::string>>> futures;
            for (auto dep : parallel_deps) {
                // Launch fetch in parallel
                futures.push_back(std::async(std::launch::async, [dep, build_dir, &lockfile_deps, &parse_args]() {
                    return fetchDependency(dep, build_dir, lockfile_deps, parse_args);
                }));
            }

            // Wait for all fetches and collect errors
            std::vector<std::string> errors;
            for (auto &f : futures) {
                if (auto res = f.get(); !res) {
                    errors.push_back(res.error());
                }
            }

            if (!errors.empty()) {
                std::string combined_error = "Parallel fetch failed with the following errors:\n";
                for (const auto &err : errors) {
                    combined_error += " - " + err + "\n";
                }
                return std::unexpected(combined_error);
            }
        }

        // 4. Execute serial-only (vcpkg, local, workspace link)
        // Maintain fail-fast semantics for these
        for (auto dep : serial_deps) {
            if (auto res = fetchDependency(dep, build_dir, lockfile_deps, parse_args); !res) {
                return res; // Fail fast
            }
        }
    }

    catalyst::logger.debug("Running post-fetch hooks.");
    if (auto res = hooks::postFetch(config); !res) {
        return res;
    }

    catalyst::logger.debug("Fetch subcommand finished successfully.");
    return {};
}
} // namespace catalyst::fetch
