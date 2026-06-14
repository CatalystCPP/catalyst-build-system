#include <cctype>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "catalyst/utils/result.hpp"
#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/fetch.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::fetch {
namespace fs = std::filesystem;

std::string getCompilerVersion(const std::string &compiler_exe) {
    auto res = catalyst::processExecStdout({compiler_exe, "--version"});
    if (!res)
        return "";
    const std::string &out = *res;
    size_t pos = out.find("version");
    if (pos == std::string::npos) {
        pos = out.find(' ');
    }
    size_t digit_pos = std::string::npos;
    for (size_t i = (pos != std::string::npos ? pos : 0); i < out.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(out[i]))) {
            digit_pos = i;
            break;
        }
    }
    if (digit_pos == std::string::npos)
        return "";
    size_t end_pos = digit_pos;
    while (end_pos < out.size() && (std::isdigit(static_cast<unsigned char>(out[end_pos])) || out[end_pos] == '.')) {
        end_pos++;
    }
    std::string version = out.substr(digit_pos, end_pos - digit_pos);
    size_t dot = version.find('.');
    if (dot != std::string::npos) {
        return version.substr(0, dot);
    }
    return version;
}

Result<void> fetchConanDeps(const std::vector<ryml::ConstNodeRef> &conan_deps,
                                                const std::string &build_dir,
                                                const utils::yaml::Configuration &config,
                                                const std::vector<std::string> &profiles) {
    namespace yaml = utils::yaml;
    catalyst::logger.debug("Writing conanfile.txt in {}", build_dir);
    fs::path build_path(build_dir);
    fs::create_directories(build_path);
    fs::path conanfile_path = build_path / "conanfile.txt";
    std::ofstream conanfile(conanfile_path);
    if (!conanfile.is_open()) {
        return std::unexpected(std::format("Failed to open {} for writing", conanfile_path.string()));
    }

    conanfile << "[requires]\n";
    for (ryml::ConstNodeRef dep : conan_deps) {
        auto name = yaml::asString(yaml::child(dep, "name"));
        auto version = yaml::asString(yaml::child(dep, "version"));
        if (!name || !version) {
            return std::unexpected("Conan dependency is missing 'name' or 'version'.");
        }
        conanfile << *name << "/" << *version << "\n";
    }

    conanfile << "\n[generators]\n";
    conanfile << "PkgConfigDeps\n";
    conanfile.close();

    catalyst::logger.info("Installing Conan dependencies...");

    // Build conan command
    std::vector<std::string> conan_cmd = {"conan",
                                          "install",
                                          build_dir,
                                          "--output-folder=" + (build_path / "conan").string(),
                                          "--build=missing",
                                          "-g",
                                          "PkgConfigDeps"};

    // 1. Determine build type
    std::string build_type = "Release";
    for (const auto &profile : profiles) {
        if (profile == "debug") {
            build_type = "Debug";
            break;
        }
    }
    conan_cmd.push_back("-s");
    conan_cmd.push_back("build_type=" + build_type);

    // 2. Determine compiler
    auto cxx_opt = config.getString("manifest.tooling.CXX");
    if (cxx_opt && !cxx_opt->empty()) {
        std::string cxx = *cxx_opt;
        std::string compiler = "";
        if (cxx.find("clang++") != std::string::npos || cxx.find("clang") != std::string::npos) {
            compiler = "clang";
        } else if (cxx.find("g++") != std::string::npos || cxx.find("gcc") != std::string::npos) {
            compiler = "gcc";
        } else if (cxx.find("cl") != std::string::npos || cxx.find("MSVC") != std::string::npos) {
            compiler = "msvc";
        }

        if (!compiler.empty()) {
            conan_cmd.push_back("-s");
            conan_cmd.push_back("compiler=" + compiler);

            std::string version = getCompilerVersion(cxx);
            if (!version.empty()) {
                conan_cmd.push_back("-s");
                conan_cmd.push_back("compiler.version=" + version);
            }

            if (compiler == "gcc" || compiler == "clang") {
#if defined(__linux__)
                conan_cmd.push_back("-s");
                conan_cmd.push_back("compiler.libcxx=libstdc++11");
#endif
            }

            conan_cmd.push_back("-s");
            conan_cmd.push_back("compiler.cppstd=23");
        }
    }

    std::string cmd_str;
    for (const auto &arg : conan_cmd) {
        cmd_str += arg + " ";
    }
    catalyst::logger.debug("Executing Conan command: {}", cmd_str);

    auto res = catalyst::processExec(std::move(conan_cmd));
    if (!res) {
        return std::unexpected(res.error());
    }

    if (res.value().get() != 0) {
        return std::unexpected("Conan installation failed.");
    }

    return {};
}

namespace {

Result<void> fetchVcpkg(const std::string &name, const std::string &triplet) {
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

Result<void> fetchGit(
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

Result<void> fetchSystem(const std::string &name) {
    // assuming installed on system
    catalyst::logger.debug("Skipping fetch for system dependency: {}", name);
    return {};
}

struct FetchLocalArgs {
    std::string name;
    std::string path;
    std::vector<std::string> profiles;
};

Result<void> fetchLocal(const FetchLocalArgs &fn_args) {
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

Result<void> fetchDependency(ryml::ConstNodeRef dep,
                                                 const std::string &build_dir,
                                                 const std::unordered_map<std::string, LockedDep> &lockfile_deps,
                                                 const Parse &parse_args) {
    namespace yaml = utils::yaml;
    auto name = yaml::asString(yaml::child(dep, "name")).value_or("");
    auto source = yaml::asString(yaml::child(dep, "source")).value_or("");

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
        else if (auto v = yaml::asString(yaml::child(dep, "version")))
            version = *v;
        else
            return std::unexpected(std::format("vcpkg dependency '{}' is missing version.", name));

        std::string triplet;
        if (!locked_triplet.empty())
            triplet = locked_triplet;
        else if (auto t = yaml::asString(yaml::child(dep, "triplet")))
            triplet = *t;
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
        else if (auto p = yaml::asString(yaml::child(dep, "path")))
            path = *p;
        else
            return std::unexpected(std::format("Local dependency '{}' is missing path.", name));

        std::vector<std::string> profiles_vec =
            yaml::asStringVector(yaml::child(dep, "profiles")).value_or(std::vector<std::string>{});
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
            else if (auto v = yaml::asString(yaml::child(dep, "version")))
                version = *v;
            else
                version = "latest";

            std::string url;
            if (!locked_url.empty())
                url = locked_url;
            else if (auto u = yaml::asString(yaml::child(dep, "url")); source == "git" && u)
                url = *u;
            else
                url = source;

            if (auto res = fetchGit(build_dir, name, url, version, locked_hash); !res)
                return std::unexpected(res.error());
        }
    }
    return {};
}

} // namespace

Result<void> action(const Parse &parse_args) {
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
        namespace yaml = utils::yaml;
        catalyst::logger.info("Using lockfile at {}", lockfile_path.string());
        auto lock_tree = yaml::loadFile(lockfile_path);
        if (!lock_tree) {
            catalyst::logger.warn("Failed to load lockfile: {}", lock_tree.error());
        } else if (ryml::ConstNodeRef lock_deps = yaml::child(lock_tree->crootref(), "dependencies");
                   lock_deps.readable() && lock_deps.is_seq()) {
            for (ryml::ConstNodeRef dep : lock_deps.children()) {
                auto dep_name = yaml::asString(yaml::child(dep, "name"));
                if (!dep_name)
                    continue;
                LockedDep ld;
                ld.hash = yaml::asString(yaml::child(dep, "hash")).value_or("");
                ld.url = yaml::asString(yaml::child(dep, "url")).value_or("");
                ld.version = yaml::asString(yaml::child(dep, "version")).value_or("");
                ld.triplet = yaml::asString(yaml::child(dep, "triplet")).value_or("");
                ld.path = yaml::asString(yaml::child(dep, "path")).value_or("");
                lockfile_deps[*dep_name] = ld;
            }
        }
    }

    namespace yaml = utils::yaml;
    std::string build_dir = config.getBuildDir().string();
    if (auto deps = yaml::child(config.rootRef(), "dependencies"); deps.readable() && deps.is_seq()) {
        std::vector<ryml::ConstNodeRef> parallel_deps;
        std::vector<ryml::ConstNodeRef> serial_deps;
        std::vector<ryml::ConstNodeRef> conan_deps;
        std::unordered_set<std::string> seen_deps;

        for (int ii = 0; ryml::ConstNodeRef dep : deps.children()) {
            auto name_opt = yaml::asString(yaml::child(dep, "name"));
            if (!name_opt) {
                return std::unexpected(std::format("Dependency: {} does not define field: name", ii));
            }
            std::string name = *name_opt;
            auto source_opt = yaml::asString(yaml::child(dep, "source"));
            if (!source_opt) {
                catalyst::logger.error("Dependency: {} does not define field: source", name);
                return std::unexpected(std::format("Dependency: {} does not define field: source", name));
            }

            // 1. Deduplicate by logical target name
            if (seen_deps.contains(name)) {
                catalyst::logger.debug("Skipping duplicate dependency entry: {}", name);
                ++ii;
                continue;
            }
            seen_deps.insert(name);

            std::string source = *source_opt;
            bool is_workspace_member = parse_args.workspace && parse_args.workspace->findPackage(name).has_value();

            // 2. Categorize by safety
            // Workspace links, vcpkg installs, and local builds mutate shared state
            if (source == "conan") {
                conan_deps.push_back(dep);
            } else if (is_workspace_member || source == "vcpkg" || source == "local") {
                serial_deps.push_back(dep);
            } else {
                parallel_deps.push_back(dep);
            }
            ++ii;
        }

        // 3. Execute parallel-safe (git, system)
        if (!parallel_deps.empty()) {
            std::vector<std::future<Result<void>>> futures;
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

        // Execute Conan dependencies seperately from other dependencies since conan needs compiler info and stuff
        if (!conan_deps.empty()) {
            if (auto res = fetchConanDeps(conan_deps, build_dir, config, parse_args.profiles); !res) {
                return res;
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
