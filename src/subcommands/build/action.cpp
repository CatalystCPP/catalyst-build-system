#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "catalyst/globals.hpp"
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
            catalyst::logger.explain("Build failed: '{}'. Invoking on-build-failure hook.", result.error());
            if (auto hook_res = hooks::onBuildFailure(config); !hook_res) {
                catalyst::logger.explain("on-build-failure hook failed: '{}' (original error: '{}')", hook_res.error(),
                                         result.error());
                catalyst::logger.error("on_build_failure hook failed: {}", hook_res.error());
                result =
                    std::unexpected(result.error() + "\nAdditionally, the on_build_failure hook failed with error: "
                                    + hook_res.error());
            } else {
                catalyst::logger.explain("on-build-failure hook succeeded.");
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

std::vector<std::string> workspaceProfiles(const WorkspaceMember &member, const Parse &args) {
    if (args.profiles.empty() || (args.profiles.size() == 1 && args.profiles.front() == "common"))
        return member.profiles.empty() ? std::vector<std::string>{"common"} : member.profiles;
    return args.profiles;
}

auto join_vec = [](const auto &vec, std::string_view delim = ", ") -> std::string {
    std::string res;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0)
            res += delim;
        res += vec[i];
    }
    return res;
};

Result<WorkspaceBuildGraph> workspaceBuildGraph(const Workspace &ws, const Parse &args) {
    WorkspaceBuildGraph graph;
    for (const auto &[key, member] : ws.getMembers()) {
        try {
            const auto profiles = workspaceProfiles(member, args);

            utils::yaml::Configuration config(profiles, member.path);
            auto name_opt = config.getString("manifest.name");
            if (!name_opt) {
                catalyst::logger.warn("Member {} has no manifest.name", key);
                continue;
            }
            const std::string &name = *name_opt;
            catalyst::logger.explain("Discovered workspace member '{}' at '{}' (configured profiles: [{}]).",
                                     name, member.path.string(), join_vec(profiles));

            PackageInfo info;
            info.name = name;
            info.member = member;

            namespace yaml = utils::yaml;
            if (ryml::ConstNodeRef deps = yaml::child(config.rootRef(), "dependencies");
                deps.readable() && deps.is_seq()) {
                for (ryml::ConstNodeRef dep : deps.children()) {
                    if (auto dep_name = yaml::asString(yaml::child(dep, "name"))) {
                        catalyst::logger.explain("Workspace dependency edge: member '{}' declares dependency on '{}'.",
                                                 name, *dep_name);
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

    catalyst::logger.explain("Workspace topological build order: [{}]", join_vec(graph.build_order, " -> "));
    return graph;
}

Result<std::unordered_set<std::string>> workspaceTargets(const WorkspaceBuildGraph &graph,
                                                         const std::string &requested_package) {
    if (requested_package.empty()) {
        std::unordered_set<std::string> targets;
        targets.reserve(graph.packages.size());
        for (const auto &package : graph.packages)
            targets.insert(package.first);
        catalyst::logger.explain("Workspace target selection: all {} package(s) selected.", targets.size());
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

    std::string target_names;
    std::string excluded_names;
    for (const auto &[name, _] : graph.packages) {
        if (targets.contains(name)) {
            if (!target_names.empty())
                target_names += ", ";
            target_names += name;
        } else {
            if (!excluded_names.empty())
                excluded_names += ", ";
            excluded_names += name;
        }
    }
    catalyst::logger.explain("Workspace target selection: requested package '{}', dependency closure: [{}], excluded members: [{}]",
                             requested_package, target_names, excluded_names);
    return targets;
}

std::vector<std::string> workspaceMemberBuildCommand(const Parse &args) {
    std::vector<std::string> command{args.executable_path.string(), "build"};
    if (args.explain)
        command.emplace_back("--explain");
    if (args.regen)
        command.emplace_back("--regen");
    if (args.force_rebuild)
        command.emplace_back("--force-rebuild");
    if (args.force_refetch)
        command.emplace_back("--force-refetch");
    for (const auto &profile : args.profiles)
        command.push_back("--profiles=" + profile);
    for (const auto &feature : args.enabled_features)
        command.push_back("--features=" + feature);
    if (!args.backend.empty()) {
        command.emplace_back("--backend");
        command.push_back(args.backend);
    }
    return command;
}

Result<void> buildWorkspaceMember(const PackageInfo &package, Parse args) {
    args.profiles = workspaceProfiles(package.member, args);

    catalyst::logger.info("Building workspace member: {}", package.name);
    std::unordered_map<std::string, std::string> environment{{"CATALYST_MACHINE", "1"}};
    if (catalyst::logger.getVerboseLogging())
        environment["CATALYST_VERBOSE"] = "1";
    std::unordered_map<std::string, std::string> child_env;
    if (catalyst::logger.explainEnabled()) {
        child_env = catalyst::logger.explainChildEnvironment(package.name);
        environment.insert(child_env.begin(), child_env.end());
    }

    auto process =
        catalyst::processExec(workspaceMemberBuildCommand(args), package.member.path.string(), std::move(environment));
    if (!process) {
        if (catalyst::logger.explainEnabled()) {
            catalyst::logger.collectExplainChild(child_env, package.name, std::nullopt);
        }
        return std::unexpected(std::format("Failed to start workspace member '{}': {}", package.name, process.error()));
    }

    const int exit_code = process->get();
    if (catalyst::logger.explainEnabled()) {
        catalyst::logger.collectExplainChild(child_env, package.name, exit_code);
    }
    if (exit_code != 0)
        return std::unexpected(
            std::format("Workspace member '{}' build exited with code {}.", package.name, exit_code));
    return {};
}

/// Validate consumer requirements against the configurations actually scheduled for workspace builds.
[[nodiscard]] Result<void> validateWorkspaceRequirements(const WorkspaceBuildGraph &graph,
                                                         const std::unordered_set<std::string> &targets,
                                                         const Parse &args) {
    namespace yaml = utils::yaml;
    try {
        for (const auto &name : graph.build_order) {
            if (!targets.contains(name))
                continue;
            const auto &package = graph.packages.at(name);
            yaml::Configuration config(workspaceProfiles(package.member, args), package.member.path);
            auto deps = yaml::child(config.rootRef(), "dependencies");
            if (!deps.readable() || !deps.is_seq())
                continue;
            for (auto dep : deps.children()) {
                const auto dependency_name = yaml::asString(yaml::child(dep, "name")).value_or("");
                if (yaml::asString(yaml::child(dep, "source")) != "local" || !graph.packages.contains(dependency_name))
                    continue;
                const auto &member = graph.packages.at(dependency_name).member;
                const auto scheduled_profiles = workspaceProfiles(member, args);
                auto requested_profiles =
                    yaml::asStringVector(yaml::child(dep, "profiles")).value_or(std::vector<std::string>{});
                if (requested_profiles.empty())
                    requested_profiles.emplace_back("common");
                yaml::Configuration scheduled(scheduled_profiles, member.path);
                yaml::Configuration requested(requested_profiles, member.path);
                auto actual_features = generate::resolveFeatureFlags(scheduled, args.enabled_features);
                auto wanted_features = generate::resolveFeatureFlags(
                    requested, yaml::asStringVector(yaml::child(dep, "using")).value_or(std::vector<std::string>{}));
                if (!actual_features || !wanted_features)
                    return std::unexpected(
                        std::format("Cannot validate workspace dependency '{} -> {}': {}",
                                    name,
                                    dependency_name,
                                    !actual_features ? actual_features.error() : wanted_features.error()));
                const auto actual = generate::featureDefinitions(
                    scheduled.getString("manifest.name").value_or(dependency_name), *actual_features);
                const auto wanted = generate::featureDefinitions(
                    requested.getString("manifest.name").value_or(dependency_name), *wanted_features);
                if (actual == wanted && scheduled_profiles == requested_profiles)
                    continue;
                std::string message =
                    std::format("Configuration mismatch for workspace member '{}', requested by '{} -> {}':\n"
                                "  Workspace build profiles: {}\n  Dependency request profiles: {}\n",
                                dependency_name,
                                name,
                                dependency_name,
                                scheduled_profiles,
                                requested_profiles);
                auto names = actual;
                names.insert(wanted.begin(), wanted.end());
                for (const auto &[macro, ignored] : names) {
                    const auto built_value = actual.contains(macro) ? actual.at(macro) : "<undefined>";
                    const auto requested_value = wanted.contains(macro) ? wanted.at(macro) : "<undefined>";
                    if (built_value != requested_value)
                        message += std::format(
                            "  {}: workspace build={}, dependency request={}\n", macro, built_value, requested_value);
                }
                message += "Align WORKSPACE.yaml profiles and dependency profiles/using overrides. "
                           "Catalyst cannot link one workspace build with incompatible consumer requirements.";
                return std::unexpected(std::move(message));
            }
        }
    } catch (const std::exception &error) {
        return std::unexpected(std::format("Failed to validate workspace dependency configurations: {}", error.what()));
    }
    return {};
}

Result<void> buildWorkspace(const WorkspaceBuildGraph &graph,
                            const std::unordered_set<std::string> &targets,
                            const Parse &parse_args) {
    using BuildFuture = std::shared_future<Result<void>>;
    std::unordered_map<std::string, BuildFuture> futures;
    std::vector<std::string> dispatched;
    std::optional<std::string> dispatch_error;

    std::vector<std::string> independent_pkgs;
    for (const auto &pkg : graph.build_order) {
        if (!targets.contains(pkg))
            continue;
        bool has_prereq = false;
        for (const auto &dep : graph.packages.at(pkg).dependencies) {
            if (targets.contains(dep)) {
                has_prereq = true;
                break;
            }
        }
        if (!has_prereq)
            independent_pkgs.push_back(pkg);
    }
    catalyst::logger.explain("Workspace concurrency planning: independent packages eligible to build concurrently: [{}]",
                             join_vec(independent_pkgs));

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
                                    catalyst::logger.explain(
                                        "Workspace member '{}' skipped because prerequisite '{}' failed.",
                                        package.name, dependency_name);
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
        if (auto res = catalyst::processExec({"cob", "-C", build_dir, "-t", "compdb"}); !res) {
            catalyst::logger.explain("Failed to generate compile commands database via cob: {}", res.error());
            return std::unexpected(res.error());
        }
        catalyst::logger.explain("Compilation database generated via cob at: '{}'",
                                 fs::absolute(build_dir / "compile_commands.json").string());
        return {};
    }
    if (generator == "ninja") {
        catalyst::logger.info("Generating compile commands database.");
        auto res = catalyst::processExecStdout(
            {"ninja", "-C", build_dir.string(), "-t", "compdb", "cc_compile", "cxx_compile"});
        if (!res) {
            catalyst::logger.explain("Failed to generate compile commands database via ninja: {}", res.error());
            return std::unexpected(res.error());
        }

        fs::path real_compdb_path = build_dir / "compile_commands.json";
        std::ofstream compdb_file{real_compdb_path};
        if (compdb_file.is_open()) {
            compdb_file << *res << std::flush;
            catalyst::logger.explain("Compilation database generated via ninja at: '{}'",
                                     fs::absolute(real_compdb_path).string());
        } else {
            return std::unexpected(std::format("Failed to open {} for writing", real_compdb_path.string()));
        }
        return {};
    }
    if (generator == "gmake" || generator == "make") {
        catalyst::logger.explain("Compilation database was not created: automatic generation is not supported for Makefiles.");
        catalyst::logger.warn("Automatic compile commands generation is not supported for Makefiles. Skipping.");
    }
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
    catalyst::logger.setExplainContext(parse_args.package, parse_args.profiles);

    if (catalyst::logger.explainEnabled()) {
        catalyst::logger.explain("Catalyst build invoked (version: {}).", catalyst::CATALYST_VERSION);
        catalyst::logger.explain("Working directory: '{}'.", fs::current_path().string());
        catalyst::logger.explain("Invocation arguments: {}", catalyst::build::describe(parse_args));

        static constexpr std::array<const char *, 9> s_allowlisted_env = {
            "CATALYST_HOOK", "CATALYST_HOOK_NAME", "CATALYST_WORKSPACE_ROOT",
            "CATALYST_PROFILES", "CATALYST_FEATURES", "CATALYST_BUILD_DIR",
            "CATALYST_INTROSPECT_FILE", "CATALYST_MACHINE", "CATALYST_VERBOSE"
        };
        std::vector<std::string> env_entries;
        for (const char *var : s_allowlisted_env) {
            if (const char *val = std::getenv(var)) {
                env_entries.push_back(std::format("{}='{}'", var, val));
            }
        }
        if (env_entries.empty()) {
            catalyst::logger.explain("Official environment variables: none set.");
        } else {
            catalyst::logger.explain("Official environment variables: [{}]", join_vec(env_entries));
        }

        if (parse_args.workspace) {
            catalyst::logger.explain("Workspace root discovered: '{}'.", parse_args.workspace->getRoot().string());
        } else {
            catalyst::logger.explain("No workspace detected (single project mode).");
        }
    }

    if (parse_args.workspace) {
        bool is_root = false;
        try {
            is_root = fs::equivalent(parse_args.workspace->getRoot(), fs::current_path());
        } catch (...) {
        }

        if (parse_args.workspace_build || is_root || !parse_args.package.empty()) {
            if (parse_args.watch)
                return std::unexpected("Workspace builds do not support --watch.");

            catalyst::logger.info("Resolving workspace build order.");
            auto graph = workspaceBuildGraph(*parse_args.workspace, parse_args);
            if (!graph)
                return std::unexpected(graph.error());
            auto targets = workspaceTargets(*graph, parse_args.package);
            if (!targets)
                return std::unexpected(targets.error());
            if (auto validation = validateWorkspaceRequirements(*graph, *targets, parse_args); !validation)
                return validation;
            auto build_res = buildWorkspace(*graph, *targets, parse_args);
            if (catalyst::logger.explainEnabled()) {
                if (build_res)
                    catalyst::logger.explain("Workspace build finished successfully.");
                else
                    catalyst::logger.explain("Workspace build failed: {}", build_res.error());
            }
            return build_res;
        }
    }

    catalyst::logger.debug("Composing profiles.");
    utils::yaml::ExplainCompositionGuard comp_guard(catalyst::logger.explainEnabled());
    utils::yaml::Configuration config{parse_args.profiles};

    if (catalyst::logger.explainEnabled()) {
        if (auto min_ver = config.getString("meta.min_ver")) {
            catalyst::logger.explain("Manifest declared minimum Catalyst version: '{}' (current: '{}').", *min_ver,
                                     catalyst::CATALYST_VERSION);
        }
    }

    Result<void> result;

    auto run_build = [&]() -> void {
        // Watch-mode iterations must not reuse stale composed feature values.
        utils::yaml::ExplainCompositionGuard inner_guard(catalyst::logger.explainEnabled());
        config = utils::yaml::Configuration{parse_args.profiles};
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
        std::string generator_source = !parse_args.backend.empty() ? "CLI override (--backend)" :
            (config.getString("meta.generator") ? "manifest (meta.generator)" : "default fallback");
        std::string build_filename = catalyst::generate::buildFilename(generator);
        fs::path build_file_path = build_dir / build_filename;

        catalyst::logger.explain("Build generator: '{}' (selected by {}).", generator, generator_source);
        catalyst::logger.explain("Expected generated build file: '{}'.", fs::absolute(build_file_path).string());

        // Local dependencies need an incremental build check on every invocation.
        // Fetch first so generation never consumes missing/stale dependency metadata.
        const auto fetch_sentinel = build_dir / ".catalyst_fetched";
        const bool needs_fetch = !fs::exists(fetch_sentinel) || parse_args.force_refetch || depMissing(config);
        if (parse_args.force_refetch) {
            catalyst::logger.explain("Forced refetch requested: removing '{}' and '{}'",
                                     fs::absolute(build_dir / "catalyst-libs").string(),
                                     fs::absolute(fetch_sentinel).string());
            fs::remove_all(build_dir / "catalyst-libs");
            fs::remove(fetch_sentinel);
        }
        if (auto res = catalyst::fetch::action(
                {.profiles = parse_args.profiles, .workspace = parse_args.workspace, .local_only = !needs_fetch});
            !res) {
            result = std::unexpected(res.error());
            return;
        }
        if (needs_fetch) {
            fs::create_directories(build_dir);
            std::ofstream{fetch_sentinel};
        }

        auto state = catalyst::generate::generationState(config, parse_args.enabled_features);
        if (!state) {
            result = std::unexpected(state.error());
            return;
        }
        std::ifstream state_file{build_dir / catalyst::generate::GENERATION_STATE_FILENAME, std::ios::binary};
        const std::string stored_state{std::istreambuf_iterator<char>{state_file}, std::istreambuf_iterator<char>{}};
        bool needs_regen = false;
        std::string regen_reason;

        if (parse_args.regen) {
            needs_regen = true;
            regen_reason = "explicit '--regen' flag requested";
            catalyst::logger.explain("Build file regeneration required: {}.", regen_reason);
        } else if (!fs::exists(build_file_path)) {
            needs_regen = true;
            regen_reason = std::format("generated build file '{}' does not exist", build_file_path.string());
            catalyst::logger.explain("Build file regeneration required: {}.", regen_reason);
        } else if (!state_file) {
            needs_regen = true;
            regen_reason = "generation state file missing or unreadable";
            catalyst::logger.explain("Build file regeneration required: {}.", regen_reason);
        } else if (stored_state != *state) {
            needs_regen = true;
            regen_reason = "generation state differs from current configuration/source state";
            catalyst::logger.explain("Build file regeneration required: {}.", regen_reason);
        } else {
            const fs::path toolchain_store = build_dir / catalyst::toolchain::RESOLVED_TOOLCHAIN_STORE_FILENAME;
            auto toolchain_changed = toolchainChanged(config, toolchain_store, generator);
            if (!toolchain_changed) {
                catalyst::logger.error("Failed to resolve toolchain state: {}", toolchain_changed.error());
                result = std::unexpected(toolchain_changed.error());
                return;
            }
            if (*toolchain_changed) {
                needs_regen = true;
                regen_reason = "resolved toolchain store missing or changed";
                catalyst::logger.explain("Build file regeneration required: {}.", regen_reason);
            } else {
                auto build_time = fs::last_write_time(build_file_path);
                if (fs::exists("CATALYST.yaml") && fs::last_write_time("CATALYST.yaml") > build_time) {
                    needs_regen = true;
                    regen_reason = "manifest 'CATALYST.yaml' is newer than build file";
                    catalyst::logger.explain("Build file regeneration required: {}.", regen_reason);
                } else {
                    for (const auto &profile : parse_args.profiles) {
                        fs::path profile_path =
                            (profile == "common") ? "catalyst.yaml" : std::format("catalyst_{}.yaml", profile);
                        if (fs::exists(profile_path) && fs::last_write_time(profile_path) > build_time) {
                            needs_regen = true;
                            regen_reason = std::format("profile manifest '{}' is newer than build file", profile_path.string());
                            catalyst::logger.explain("Build file regeneration required: {}.", regen_reason);
                            break;
                        }
                    }
                }
            }
        }

        if (!needs_regen) {
            catalyst::logger.explain("Build file regeneration skipped: existing generated build file is up to date.");
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
                catalyst::logger.explain("Published compilation database to: '{}'", fs::absolute(stable_compdb).string());
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
                    catalyst::logger.explain("Published compilation database to project root: '{}'",
                                             fs::absolute(root_compdb).string());
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
    if (!result && !parse_args.watch) {
        catalyst::logger.explain("Build failed: {}", result.error());
        return result;
    }

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
    catalyst::logger.explain("Build finished successfully.");
    return {};
}

} // namespace catalyst::build
