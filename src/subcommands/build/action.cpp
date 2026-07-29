#include <algorithm>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/build.hpp"
#include "catalyst/subcommands/fetch.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/toolchain.hpp"
#include "catalyst/utils/watcher.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/workspace.hpp"

namespace catalyst::build {
namespace fs = std::filesystem;

namespace {

struct BuildFailureGuard {
    const utils::yaml::Configuration &config;
    Result<void> &result;

    BuildFailureGuard(const BuildFailureGuard &) = delete;
    BuildFailureGuard(BuildFailureGuard &&) = delete;
    BuildFailureGuard &operator=(const BuildFailureGuard &) = delete;
    BuildFailureGuard &operator=(BuildFailureGuard &&) = delete;
    BuildFailureGuard(const utils::yaml::Configuration &cfg, Result<void> &res) : config(cfg), result(res) {
    }

    ~BuildFailureGuard() {
        if (!result) {
            if (auto hook_res = hooks::onBuildFailure(config); !hook_res) {
                catalyst::logger.error("on_build_failure hook failed: {}", hook_res.error());
                result =
                    std::unexpected(result.error() + "\nAdditionally, the on_build_failure hook failed with error: "
                                    + hook_res.error());
            }
        }
    }
};

struct PackageInfo {
    std::string name;
    WorkspaceMember member;
    std::vector<std::string> dependencies;
};

struct WorkspaceBuildGraph {
    std::unordered_map<std::string, PackageInfo> packages;
    std::vector<std::string> build_order;
};

enum class VisitState : std::uint8_t { Unvisited, Visiting, Visited };

Result<WorkspaceBuildGraph> workspaceBuildGraph(const Workspace &ws) {
    WorkspaceBuildGraph graph;
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
            info.member = member;

            namespace yaml = utils::yaml;
            if (ryml::ConstNodeRef deps = yaml::child(config.rootRef(), "dependencies");
                deps.readable() && deps.is_seq()) {
                for (ryml::ConstNodeRef dep : deps.children()) {
                    if (auto dep_name = yaml::asString(yaml::child(dep, "name"))) {
                        info.dependencies.push_back(std::move(*dep_name));
                    }
                }
            }
            if (!graph.packages.emplace(name, std::move(info)).second) {
                return std::unexpected(std::format("Workspace contains multiple members named '{}'.", name));
            }
        } catch (const std::exception &e) {
            return std::unexpected(std::format("Failed to load config for workspace member '{}': {}", key, e.what()));
        } catch (...) {
            return std::unexpected(std::format("Failed to load config for workspace member '{}'.", key));
        }
    }

    std::unordered_map<std::string, VisitState> states;
    std::vector<std::string> active_path;
    std::function<Result<void>(const std::string &)> visit = [&](const std::string &package_name) -> Result<void> {
        const VisitState state = states[package_name];
        if (state == VisitState::Visited)
            return {};
        if (state == VisitState::Visiting) {
            auto cycle_start = std::ranges::find(active_path, package_name);
            std::string cycle;
            for (auto it = cycle_start; it != active_path.end(); ++it)
                cycle += (cycle.empty() ? "" : " -> ") + *it;
            cycle += " -> " + package_name;
            return std::unexpected("Circular workspace dependency detected: " + cycle);
        }

        states[package_name] = VisitState::Visiting;
        active_path.push_back(package_name);
        for (const auto &dependency : graph.packages.at(package_name).dependencies) {
            if (graph.packages.contains(dependency)) {
                if (auto result = visit(dependency); !result)
                    return result;
            }
        }
        active_path.pop_back();
        states[package_name] = VisitState::Visited;
        graph.build_order.push_back(package_name);
        return {};
    };

    for (const auto &package : graph.packages) {
        if (auto result = visit(package.first); !result)
            return std::unexpected(result.error());
    }

    return graph;
}

Result<std::unordered_set<std::string>> workspaceTargets(const WorkspaceBuildGraph &graph,
                                                         const std::string &requested_package) {
    if (requested_package.empty()) {
        std::unordered_set<std::string> targets;
        targets.reserve(graph.packages.size());
        for (const auto &package : graph.packages)
            targets.insert(package.first);
        return targets;
    }
    if (!graph.packages.contains(requested_package))
        return std::unexpected("Package " + requested_package + " not found in workspace.");

    std::unordered_set<std::string> targets;
    std::function<void(const std::string &)> add_with_dependencies = [&](const std::string &package_name) -> void {
        if (!targets.insert(package_name).second)
            return;
        for (const auto &dependency : graph.packages.at(package_name).dependencies)
            if (graph.packages.contains(dependency))
                add_with_dependencies(dependency);
    };
    add_with_dependencies(requested_package);
    return targets;
}

std::vector<std::string> workspaceMemberBuildCommand(const Parse &args) {
    std::vector<std::string> command{args.executable_path.string(), "build"};
    if (args.regen)
        command.emplace_back("--regen");
    if (args.force_rebuild)
        command.emplace_back("--force-rebuild");
    if (args.force_refetch)
        command.emplace_back("--force-refetch");
    if (!args.profiles.empty()) {
        command.emplace_back("--profiles");
        command.insert(command.end(), args.profiles.begin(), args.profiles.end());
    }
    if (!args.enabled_features.empty()) {
        command.emplace_back("--features");
        command.insert(command.end(), args.enabled_features.begin(), args.enabled_features.end());
    }
    if (!args.backend.empty()) {
        command.emplace_back("--backend");
        command.push_back(args.backend);
    }
    return command;
}

Result<void> buildWorkspaceMember(const PackageInfo &package, Parse args) {
    if (args.profiles.size() == 1 && args.profiles.front() == "common" && !package.member.profiles.empty())
        args.profiles = package.member.profiles;

    catalyst::logger.info("Building workspace member: {}", package.name);
    std::unordered_map<std::string, std::string> environment{{"CATALYST_MACHINE", "1"}};
    if (catalyst::logger.getVerboseLogging())
        environment["CATALYST_VERBOSE"] = "1";

    auto process =
        catalyst::processExec(workspaceMemberBuildCommand(args), package.member.path.string(), std::move(environment));
    if (!process)
        return std::unexpected(std::format("Failed to start workspace member '{}': {}", package.name, process.error()));

    const int exit_code = process->get();
    if (exit_code != 0)
        return std::unexpected(
            std::format("Workspace member '{}' build exited with code {}.", package.name, exit_code));
    return {};
}

Result<void> buildWorkspace(const WorkspaceBuildGraph &graph,
                            const std::unordered_set<std::string> &targets,
                            const Parse &parse_args) {
    using BuildFuture = std::shared_future<Result<void>>;
    std::unordered_map<std::string, BuildFuture> futures;
    std::vector<std::string> dispatched;
    std::optional<std::string> dispatch_error;

    for (const auto &package_name : graph.build_order) {
        if (!targets.contains(package_name))
            continue;

        const PackageInfo &package = graph.packages.at(package_name);
        std::vector<std::pair<std::string, BuildFuture>> dependency_futures;
        for (const auto &dependency : package.dependencies) {
            if (targets.contains(dependency) && futures.contains(dependency))
                dependency_futures.emplace_back(dependency, futures.at(dependency));
        }

        try {
            auto future =
                std::async(
                    std::launch::async,
                    [package,
                     args = parse_args,
                     dependencies = std::move(dependency_futures)]() mutable -> Result<void> {
                        try {
                            for (const auto &[dependency_name, dependency_future] : dependencies) {
                                const Result<void> &dependency_result = dependency_future.get();
                                if (!dependency_result) {
                                    return std::unexpected(std::format(
                                        "Workspace member '{}' was not built because dependency '{}' failed: "
                                        "{}",
                                        package.name,
                                        dependency_name,
                                        dependency_result.error()));
                                }
                            }
                            return buildWorkspaceMember(package, std::move(args));
                        } catch (const std::exception &error) {
                            return std::unexpected(std::format(
                                "Workspace member '{}' build threw an exception: {}", package.name, error.what()));
                        } catch (...) {
                            return std::unexpected(
                                std::format("Workspace member '{}' build threw an unknown exception.", package.name));
                        }
                    })
                    .share();
            futures.emplace(package_name, std::move(future));
            dispatched.push_back(package_name);
        } catch (const std::exception &error) {
            dispatch_error = std::format("Failed to dispatch workspace member '{}': {}", package_name, error.what());
            break;
        } catch (...) {
            dispatch_error = std::format("Failed to dispatch workspace member '{}'.", package_name);
            break;
        }
    }

    std::vector<std::string> errors;
    if (dispatch_error)
        errors.push_back(std::move(*dispatch_error));
    for (const auto &package_name : dispatched) {
        try {
            const Result<void> &result = futures.at(package_name).get();
            if (!result)
                errors.push_back(result.error());
        } catch (const std::exception &error) {
            errors.push_back(std::format("Workspace member '{}' future failed: {}", package_name, error.what()));
        } catch (...) {
            errors.push_back(std::format("Workspace member '{}' future failed.", package_name));
        }
    }

    if (errors.empty())
        return {};

    std::string message = "Workspace build failed:";
    for (const auto &error : errors)
        message += "\n - " + error;
    return std::unexpected(std::move(message));
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

Result<void> generateCompileCommands(const fs::path &build_dir, const std::string &generator) {
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

Result<bool>
toolchainChanged(const utils::yaml::Configuration &config, const fs::path &store_path, std::string_view generator) {
    std::optional<fs::path> toolchain_path;
    if (auto configured_path = config.getString("manifest.toolchain"))
        toolchain_path = *configured_path;

    auto resolved_toolchain = catalyst::toolchain::resolveToolchain(toolchain_path);
    if (!resolved_toolchain)
        return std::unexpected(resolved_toolchain.error());

    std::error_code ec;
    if (!fs::exists(store_path, ec)) {
        if (ec)
            return std::unexpected(
                std::format("Failed to inspect resolved toolchain store {}: {}", store_path.string(), ec.message()));
        return true;
    }

    std::ifstream store{store_path, std::ios::binary};
    if (!store)
        return std::unexpected(std::format("Failed to open {} for reading", store_path.string()));

    std::string stored_toolchain;
    char buffer[4096];
    while (store) {
        store.read(buffer, sizeof(buffer));
        stored_toolchain.append(buffer, static_cast<size_t>(store.gcount()));
    }
    if (!store.eof())
        return std::unexpected(std::format("Failed to read {}", store_path.string()));

    return stored_toolchain != catalyst::toolchain::serializeToolchainStore(*resolved_toolchain, generator);
}

} // namespace

Result<void> action(const Parse &parse_args) {
    catalyst::logger.debug("Build subcommand invoked.");

    if (parse_args.workspace) {
        bool is_root = false;
        try {
            is_root = fs::equivalent(parse_args.workspace->getRoot(), fs::current_path());
        } catch (...) {
            std::ignore;
        }

        if (parse_args.workspace_build || is_root || !parse_args.package.empty()) {
            if (parse_args.watch)
                return std::unexpected("Workspace builds do not support --watch.");

            catalyst::logger.info("Resolving workspace build order.");
            auto graph = workspaceBuildGraph(*parse_args.workspace);
            if (!graph)
                return std::unexpected(graph.error());
            auto targets = workspaceTargets(*graph, parse_args.package);
            if (!targets)
                return std::unexpected(targets.error());
            return buildWorkspace(*graph, *targets, parse_args);
        }
    }

    catalyst::logger.debug("Composing profiles.");
    utils::yaml::Configuration config{parse_args.profiles};

    Result<void> result;

    auto run_build = [&]() -> void {
        BuildFailureGuard guard{config, result};

        catalyst::logger.info("Running pre-build hooks.");
        if (auto res = hooks::preBuild(config); !res) {
            catalyst::logger.error("Pre-build hook failed: {}", res.error());
            result = std::unexpected(res.error());
            return;
        }

        catalyst::logger.info("Running pre-generate hooks.");
        if (auto res = hooks::preGenerate(config); !res) {
            catalyst::logger.error("Pre-generate hook failed: {}", res.error());
            result = std::unexpected(res.error());
            return;
        }

        fs::path build_dir = config.getBuildDir();
        std::string generator =
            parse_args.backend.empty() ? config.getString("meta.generator").value_or("cob") : parse_args.backend;
        std::string build_filename = catalyst::generate::buildFilename(generator);

        bool needs_regen = !fs::exists(build_dir / build_filename) || parse_args.regen;
        if (!needs_regen) {
            const fs::path toolchain_store = build_dir / catalyst::toolchain::RESOLVED_TOOLCHAIN_STORE_FILENAME;
            auto toolchain_changed = toolchainChanged(config, toolchain_store, generator);
            if (!toolchain_changed) {
                catalyst::logger.error("Failed to resolve toolchain state: {}", toolchain_changed.error());
                result = std::unexpected(toolchain_changed.error());
                return;
            }
            needs_regen = *toolchain_changed;
            if (needs_regen)
                catalyst::logger.debug("Resolved toolchain changed; build files must be regenerated.");
        }
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
                                                   .backend = parse_args.backend,
                                                   .skip_pre_generate = true});
            if (!res) {
                catalyst::logger.error("Failed to generate build files: {}", res.error());
                result = std::unexpected(res.error());
                return;
            }
        }

        const auto fetch_sentinel = build_dir / ".catalyst_fetched";
        bool needs_fetch = !fs::exists(fetch_sentinel) || parse_args.force_refetch || depMissing(config);
        if (needs_fetch) {
            if (parse_args.force_refetch) {
                catalyst::logger.info("Forcefully refetching dependencies.");
                fs::remove_all(fs::path{build_dir / "catalyst-libs"}); // cleanup
                std::error_code ec;
                fs::remove(build_dir / ".catalyst_fetched", ec);
            }
            catalyst::logger.info("Fetching dependencies.");
            if (auto res =
                    catalyst::fetch::action({.profiles = parse_args.profiles, .workspace = parse_args.workspace});
                !res) {
                catalyst::logger.error("Failed to fetch dependencies: {}", res.error());
                result = std::unexpected(res.error());
                return;
            }
            std::error_code ec;
            fs::create_directories(build_dir, ec);
            std::ofstream{fetch_sentinel}; // create sentinel file and close it via RAII
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

        // Publish compile commands to stable root path
        fs::path source_compdb = build_dir / "compile_commands.json";
        if (fs::exists(source_compdb)) {
            // 1. Stable build path (e.g. build/compile_commands.json)
            fs::path stable_compdb = build_dir.parent_path() / "compile_commands.json";
            std::error_code ec;
            if (fs::exists(stable_compdb)) {
                fs::remove(stable_compdb, ec);
            }
            fs::copy_file(source_compdb, stable_compdb, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                catalyst::logger.warn(
                    "Failed to publish compilation database to {}: {}", stable_compdb.string(), ec.message());
            } else {
                catalyst::logger.debug("Published compilation database to {}", stable_compdb.string());
            }

            // 2. Project root path (e.g. compile_commands.json)
            fs::path root_compdb = fs::current_path() / "compile_commands.json";
            if (source_compdb != root_compdb && stable_compdb != root_compdb) {
                if (fs::exists(root_compdb)) {
                    fs::remove(root_compdb, ec);
                }
                fs::copy_file(source_compdb, root_compdb, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    catalyst::logger.warn("Failed to publish compilation database to project root: {}", ec.message());
                } else {
                    catalyst::logger.debug("Published compilation database to project root");
                }
            }
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
