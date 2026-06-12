#include <algorithm>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/build.hpp"
#include "catalyst/subcommands/fetch.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/watcher.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/workspace.hpp"

namespace catalyst::build {
namespace fs = std::filesystem;

namespace {

struct BuildFailureGuard {
    const utils::yaml::Configuration &config;
    std::expected<void, std::string> &result;

    BuildFailureGuard(const BuildFailureGuard &) = delete;
    BuildFailureGuard(BuildFailureGuard &&) = delete;
    BuildFailureGuard &operator=(const BuildFailureGuard &) = delete;
    BuildFailureGuard &operator=(BuildFailureGuard &&) = delete;
    BuildFailureGuard(const utils::yaml::Configuration &cfg, std::expected<void, std::string> &res)
        : config(cfg), result(res) {
    }

    ~BuildFailureGuard() {
        if (!result) {
            if (auto hook_res = hooks::onBuildFailure(config); !hook_res) {
                catalyst::logger.error("on_build_failure hook failed: {}", hook_res.error());
                result =
                    std::unexpected(result.error() +
                                    "\nAdditionally, the on_build_failure hook failed with error: " + hook_res.error());
            }
        }
    }
};

struct PackageInfo {
    std::string name;
    std::string workspace_member_key;
    std::vector<std::string> dependencies;
};

/// Perform a topological sort on workspace members based on their dependencies
/// will look through all the packages in the workspace, read their manifests,
/// and then perform a topological sort to determine the correct build order.
std::vector<WorkspaceMember> buildOrderTopSort(const Workspace &ws) {
    std::unordered_map<std::string, PackageInfo> packages;

    for (const auto &[key, member] : ws.getMembers()) {
        try {
            std::vector<std::string> profiles = member.profiles;
            if (profiles.empty())
                profiles.emplace_back("common");

            utils::yaml::Configuration config(profiles, member.path);
            auto name_opt = config.getString("manifest.name");
            if (!name_opt) {
                catalyst::logger.warn("Member {} has no manifest.name", key);
                continue;
            }
            const std::string &name = *name_opt;

            PackageInfo info;
            info.name = name;
            info.workspace_member_key = key;

            namespace yaml = utils::yaml;
            if (ryml::ConstNodeRef deps = yaml::child(config.rootRef(), "dependencies");
                deps.readable() && deps.is_seq()) {
                for (ryml::ConstNodeRef dep : deps.children()) {
                    if (auto dep_name = yaml::asString(yaml::child(dep, "name"))) {
                        info.dependencies.push_back(std::move(*dep_name));
                    }
                }
            }
            packages[name] = info;

        } catch (const std::exception &e) {
            catalyst::logger.error("Failed to load config for member {}: {}", key, e.what());
        }
    }

    std::vector<WorkspaceMember> order;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> visiting;

    std::function<void(const std::string &)> visit = [&](const std::string &pkg_name) {
        if (visited.contains(pkg_name))
            return;
        if (visiting.contains(pkg_name)) {
            catalyst::logger.warn("Circular dependency detected involving {}", pkg_name);
            return;
        }
        visiting.insert(pkg_name);

        if (packages.contains(pkg_name)) {
            const auto &info = packages[pkg_name];
            for (const auto &dep : info.dependencies) {
                if (packages.contains(dep)) {
                    visit(dep);
                }
            }

            std::string key = info.workspace_member_key;
            auto it = ws.getMembers().find(key);
            if (it != ws.getMembers().end()) {
                order.push_back(it->second);
            }
        }
        visited.insert(pkg_name);
        visiting.erase(pkg_name);
    };

    for (const auto &[name, info] : packages) {
        visit(name);
    }

    return order;
}

bool depMissing(const utils::yaml::Configuration &config) {
    catalyst::logger.debug("Checking for missing dependencies.");
    fs::path build_dir = config.getBuildDir();
    if (!config.has("dependencies")) {
        catalyst::logger.debug("No dependencies declared, skipping check.");
        return false;
    }
    // TODO: needs to be updated to respect actual dependency types
    namespace yaml = utils::yaml;
    ryml::ConstNodeRef deps = yaml::child(config.rootRef(), "dependencies");
    if (!deps.readable() || !deps.is_seq()) {
        return false;
    }
    for (ryml::ConstNodeRef dep : deps.children()) {
        if (yaml::asString(yaml::child(dep, "source")) != "git") {
            continue;
        }
        std::string dep_name = yaml::asString(yaml::child(dep, "name")).value_or("");
        if (!fs::exists(build_dir / "catalyst-libs" / dep_name)) {
            catalyst::logger.warn("Missing dependency: {}", dep_name);
            return true;
        }
    }
    return false;
}

std::expected<void, std::string> generateCompileCommands(const fs::path &build_dir, const std::string &generator) {
    if (generator == "cob") {
        catalyst::logger.info("Generating compile commands database.");
        if (auto res = catalyst::processExec({"cob", "-C", build_dir, "-t", "compdb"}); !res)
            return std::unexpected(res.error());
        return {};
    }
    if (generator == "ninja") {
        catalyst::logger.info("Generating compile commands database.");
        auto res = catalyst::processExecStdout(
            {"ninja", "-C", build_dir.string(), "-t", "compdb", "cc_compile", "cxx_compile"});
        if (!res)
            return std::unexpected(res.error());

        fs::path real_compdb_path = build_dir / "compile_commands.json";
        std::ofstream compdb_file{real_compdb_path};
        if (compdb_file.is_open())
            compdb_file << *res << std::flush;
        else
            return std::unexpected(std::format("Failed to open {} for writing", real_compdb_path.string()));
        return {};
    }
    if (generator == "gmake" || generator == "make")
        catalyst::logger.warn("Automatic compile commands generation is not supported for Makefiles. Skipping.");
    return {}; // don't fail if we don't know how to generate compile commands for this generator, it's not critical
}

} // namespace

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.debug("Build subcommand invoked.");

    if (parse_args.workspace) {
        fs::path current = fs::current_path();
        bool is_root = false;
        try {
            is_root = fs::equivalent(parse_args.workspace->getRoot(), current);
        } catch (...) {
            std::ignore;
        }

        if (parse_args.workspace_build || is_root || !parse_args.package.empty()) {
            catalyst::logger.info("Resolving workspace build order.");
            auto order = buildOrderTopSort(*parse_args.workspace);

            std::vector<WorkspaceMember> targets;
            if (!parse_args.package.empty()) {
                bool found = false;
                for (const auto &m : order) {
                    utils::yaml::Configuration c(m.profiles.empty() ? std::vector<std::string>{"common"} : m.profiles,
                                                 m.path);
                    if (c.getString("manifest.name").value_or("") == parse_args.package) {
                        targets.push_back(m);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return std::unexpected("Package " + parse_args.package + " not found in workspace.");
            } else {
                targets = order;
            }

            for (const auto &member : targets) {
                catalyst::logger.info("Building workspace member: {}", member.name);
                fs::current_path(member.path);

                Parse member_args = parse_args;
                member_args.workspace_build = false;
                member_args.package = ""; // Clear package arg so we don't recurse logic

                // Profile logic: if default "common", use member profiles
                if (member_args.profiles.size() == 1 && member_args.profiles[0] == "common") {
                    if (!member.profiles.empty()) {
                        member_args.profiles = member.profiles;
                    }
                }

                auto res = action(member_args);
                fs::current_path(current); // Restore

                if (!res)
                    return res;
            }
            return {};
        }
    }

    catalyst::logger.debug("Composing profiles.");
    utils::yaml::Configuration config{parse_args.profiles};

    std::expected<void, std::string> result;

    auto run_build = [&]() -> void {
        BuildFailureGuard guard{config, result};

        catalyst::logger.info("Running pre-build hooks.");
        if (auto res = hooks::preBuild(config); !res) {
            catalyst::logger.error("Pre-build hook failed: {}", res.error());
            result = std::unexpected(res.error());
            return;
        }

        fs::path build_dir = config.getBuildDir();
        std::string generator = config.getString("meta.generator").value_or("cob");
        std::string build_filename = (generator == "ninja") ? "build.ninja" : "catalyst.build";

        bool needs_regen = !fs::exists(build_dir / build_filename) || parse_args.regen;
        if (!needs_regen) {
            auto build_time = fs::last_write_time(build_dir / build_filename);
            if (fs::exists("CATALYST.yaml") && fs::last_write_time("CATALYST.yaml") > build_time) {
                needs_regen = true;
            } else {
                for (const auto &profile : parse_args.profiles) {
                    fs::path profile_path =
                        (profile == "common") ? "catalyst.yaml" : std::format("catalyst_{}.yaml", profile);
                    if (fs::exists(profile_path) && fs::last_write_time(profile_path) > build_time) {
                        needs_regen = true;
                        break;
                    }
                }
            }
        }

        if (needs_regen) {
            catalyst::logger.info("Generating build files.");
            auto res = catalyst::generate::action({.profiles = parse_args.profiles,
                                                   .enabled_features = parse_args.enabled_features,
                                                   .backend = parse_args.backend});
            if (!res) {
                catalyst::logger.error("Failed to generate build files: {}", res.error());
                result = std::unexpected(res.error());
                return;
            }
        } else {
            catalyst::logger.info("Running pre-generate hooks.");
            if (auto res = hooks::preGenerate(config); !res) {
                catalyst::logger.error("Pre-generate hook failed: {}", res.error());
                result = std::unexpected(res.error());
                return;
            }
        }

        if (!fs::exists(build_dir / "catalyst-libs") || parse_args.force_refetch || depMissing(config)) {
            if (parse_args.force_refetch) {
                catalyst::logger.info("Forcefully refetching dependencies.");
                fs::remove_all(fs::path{build_dir / "catalyst-libs"}); // cleanup
            }
            catalyst::logger.info("Fetching dependencies.");
            if (auto res =
                    catalyst::fetch::action({.profiles = parse_args.profiles, .workspace = parse_args.workspace});
                !res) {
                catalyst::logger.error("Failed to fetch dependencies: {}", res.error());
                result = std::unexpected(res.error());
                return;
            }
        }

        catalyst::logger.info("Building project.");
        std::vector<std::string> build_command = {generator, "-C", build_dir};

        if (int res = catalyst::processExec(std::move(build_command)).value().get(); res != 0) {
            catalyst::logger.error("Failed to build project.");
            result = std::unexpected(std::format("Build process failed. {} exited with code: {}", generator, res));
            return;
        }

        catalyst::logger.info("Generating compile commands.");
        if (auto res = generateCompileCommands(build_dir, generator); !res) {
            catalyst::logger.error("Failed to generate compile commands: {}", res.error());
            result = res;
            return;
        }

        catalyst::logger.info("Running post-build hooks.");
        if (auto res = hooks::postBuild(config); !res) {
            catalyst::logger.error("Post-build hook failed: {}", res.error());
            result = res;
            return;
        }
    };

    run_build();
    if (!result && !parse_args.watch)
        return result;

    if (parse_args.watch) {
        auto src_dirs = config.getStringVector("manifest.dirs.source").value_or(std::vector<std::string>{"src"});
        auto inc_dirs = config.getStringVector("manifest.dirs.include").value_or(std::vector<std::string>{"include"});

        std::vector<fs::path> watch_paths;
        watch_paths.reserve(src_dirs.size() + inc_dirs.size());
        for (const auto &d : src_dirs)
            watch_paths.push_back(fs::absolute(d));
        for (const auto &d : inc_dirs)
            watch_paths.push_back(fs::absolute(d));

        if (fs::exists("CATALYST.yaml")) {
            watch_paths.push_back(fs::absolute("CATALYST.yaml"));
        }
        for (const auto &profile : parse_args.profiles) {
            fs::path profile_path = (profile == "common") ? "catalyst.yaml" : std::format("catalyst_{}.yaml", profile);
            if (fs::exists(profile_path)) {
                watch_paths.push_back(fs::absolute(profile_path));
            }
        }

        catalyst::logger.info("Watching for changes in: {} and {}", src_dirs, inc_dirs);

        utils::watcher::Watcher watcher(watch_paths);
        watcher.watch([&](const fs::path &changed) {
            catalyst::logger.info("File changed: {}. Rebuilding...", changed.string());
            result = {}; // reset to success
            run_build();
            if (!result) {
                catalyst::logger.error("Rebuild failed: {}", result.error());
            } else {
                catalyst::logger.info("Rebuild successful.");
            }
        });
    }

    catalyst::logger.info("Build subcommand finished successfully.");
    return {};
}

} // namespace catalyst::build
