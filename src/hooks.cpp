#include "catalyst/hooks.hpp"

#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

using catalyst::Result;

namespace catalyst::hooks {
std::vector<std::string> shellCmd(std::string_view cmd) {
#if defined(_WIN32)
    return {"cmd", "/c", std::string{cmd}};
#else
    return {"/bin/sh", "-c", std::string{cmd}};
#endif
}
} // namespace catalyst::hooks

namespace {

Result<void> executeHook(const catalyst::utils::yaml::Configuration &configuration, std::string_view hook_name) {
    using catalyst::utils::yaml::asString;
    using catalyst::utils::yaml::child;

    catalyst::logger.explain("Hook lifecycle event reached: '{}'", hook_name);
    catalyst::logger.debug("Executing hook: {}", hook_name);
    ryml::ConstNodeRef hook_node = child(child(configuration.rootRef(), "hooks"), hook_name);
    if (!hook_node.readable()) {
        catalyst::logger.explain("No hook registered for lifecycle event '{}'", hook_name);
        catalyst::logger.debug("No hook defined for: {}", hook_name);
        return {};
    }

    if (auto sync_result = configuration.syncHookState(hook_name); !sync_result)
        return std::unexpected(std::format("Hook '{}' introspection setup failed: {}", hook_name, sync_result.error()));
    const catalyst::hooks::HookEnvironment environment = configuration.hookEnvironment(hook_name);

    catalyst::logger.explain("Hook tracking limitation: Catalyst tracks hook execution but cannot track undeclared inputs or opaque side effects produced by external hook commands.");
    catalyst::logger.explain("Hook working directory: '{}'", std::filesystem::current_path().string());
    std::string env_summary;
    for (const auto &[k, v] : environment) {
        if (!env_summary.empty())
            env_summary += ", ";
        env_summary += std::format("{}='{}'", k, v);
    }
    catalyst::logger.explain("Hook official environment: [{}]", env_summary);

    // Normalize: if the hook is a bare map (not wrapped in a sequence), treat it as a single-element sequence.
    std::vector<ryml::ConstNodeRef> items;
    if (hook_node.is_seq()) {
        for (ryml::ConstNodeRef item : hook_node.children())
            items.push_back(item);
    } else if (hook_node.is_map()) {
        items.push_back(hook_node);
    }

    catalyst::logger.explain("Registered {} hook action(s) for lifecycle event '{}'", items.empty() ? 1 : items.size(), hook_name);

    if (!items.empty()) {
        size_t idx = 0;
        for (ryml::ConstNodeRef item : items) {
            idx++;
            Result<void> res;
            if (child(item, "command").readable()) {
                catalyst::logger.explain("Hook action #{}/{} for '{}': type 'command'", idx, items.size(), hook_name);
                res = catalyst::hooks::executeCommandHook(item, hook_name, environment);
            } else if (child(item, "script").readable()) {
                catalyst::logger.explain("Hook action #{}/{} for '{}': type 'script'", idx, items.size(), hook_name);
                res = catalyst::hooks::executeScriptHook(item, hook_name, environment);
            } else if (child(item, "catalyst").readable()) {
                catalyst::logger.explain("Hook action #{}/{} for '{}': type 'catalyst'", idx, items.size(), hook_name);
                res = catalyst::hooks::executeCatalystHook(item, hook_name, environment);
            } else if (ryml::ConstNodeRef codegen = child(item, "codegen"); codegen.readable()) {
                catalyst::logger.explain("Hook action #{}/{} for '{}': type 'codegen'", idx, items.size(), hook_name);
                res = catalyst::hooks::executeCodegenHook(codegen, hook_name, environment);
            } else {
                return std::unexpected(std::format(
                    "Hook '{}' item is malformed. Must contain one of: 'command', 'script', 'catalyst', or 'codegen'.",
                    hook_name));
            }
            if (!res) {
                catalyst::logger.explain("Hook action #{}/{} for '{}' failed: {}", idx, items.size(), hook_name, res.error());
                return res;
            }
        }
    } else if (auto command = asString(hook_node)) {
        catalyst::logger.explain("Hook action 1/1 for '{}': type 'command' (bare command: '{}')", hook_name, *command);
        catalyst::logger.debug("[Catalyst Hook: {}] Running command: {}", hook_name, *command);
        std::vector<std::string> cmd = catalyst::hooks::shellCmd(*command);
        auto start_time = std::chrono::steady_clock::now();
        auto res = catalyst::processExec(std::move(cmd), std::nullopt, environment);
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
        if (!res || res->get()) {
            int code = res ? res->get() : -1;
            catalyst::logger.explain("Hook '{}' command exited with code {} (duration: {} ms)", hook_name, code, elapsed_ms);
            return std::unexpected(std::format("Hook '{}' command failed: {}", hook_name, *command));
        }
        catalyst::logger.explain("Hook '{}' command completed successfully in {} ms (exit code: 0)", hook_name, elapsed_ms);
    }

    catalyst::logger.debug("Hook finished successfully: {}", hook_name);
    catalyst::logger.explain("Hook lifecycle event '{}' finished successfully", hook_name);
    return {};
}

} // namespace

namespace catalyst::hooks {

Result<void> preBuild(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "pre-build");
}

Result<void> postBuild(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "post-build");
}

Result<void> onBuildFailure(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "on-build-failure");
}

Result<void> preGenerate(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "pre-generate");
}

Result<void> postGenerate(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "post-generate");
}

Result<void> preFetch(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "pre-fetch");
}

Result<void> postFetch(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "post-fetch");
}

Result<void> preClean(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "pre-clean");
}

Result<void> postClean(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "post-clean");
}

Result<void> preRun(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "pre-run");
}

Result<void> postRun(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "post-run");
}

Result<void> preTest(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "pre-test");
}

Result<void> postTest(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "post-test");
}

Result<void> preBench(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "pre-bench");
}

Result<void> postBench(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "post-bench");
}

Result<void> prePack(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "pre-pack");
}

Result<void> postPack(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp, "post-pack");
}
} // namespace catalyst::hooks
