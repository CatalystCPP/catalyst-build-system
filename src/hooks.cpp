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

    catalyst::logger.debug("Executing hook: {}", hook_name);
    ryml::ConstNodeRef hook_node = child(child(configuration.rootRef(), "hooks"), hook_name);
    if (!hook_node.readable()) {
        catalyst::logger.debug("No hook defined for: {}", hook_name);
        return {};
    }

    if (auto sync_result = configuration.syncHookState(hook_name); !sync_result)
        return std::unexpected(std::format("Hook '{}' introspection setup failed: {}", hook_name, sync_result.error()));
    const catalyst::hooks::HookEnvironment environment = configuration.hookEnvironment(hook_name);

    // Normalize: if the hook is a bare map (not wrapped in a sequence), treat it as a single-element sequence.
    std::vector<ryml::ConstNodeRef> items;
    if (hook_node.is_seq()) {
        for (ryml::ConstNodeRef item : hook_node.children())
            items.push_back(item);
    } else if (hook_node.is_map()) {
        items.push_back(hook_node);
    }

    if (!items.empty()) {
        for (ryml::ConstNodeRef item : items) {
            Result<void> res;
            if (child(item, "command").readable()) {
                res = catalyst::hooks::executeCommandHook(item, hook_name, environment);
            } else if (child(item, "script").readable()) {
                res = catalyst::hooks::executeScriptHook(item, hook_name, environment);
            } else if (child(item, "catalyst").readable()) {
                res = catalyst::hooks::executeCatalystHook(item, hook_name, environment);
            } else if (ryml::ConstNodeRef codegen = child(item, "codegen"); codegen.readable()) {
                res = catalyst::hooks::executeCodegenHook(codegen, hook_name, environment);
            } else {
                return std::unexpected(std::format(
                    "Hook '{}' item is malformed. Must contain one of: 'command', 'script', 'catalyst', or 'codegen'.",
                    hook_name));
            }
            if (!res) {
                return res;
            }
        }
    } else if (auto command = asString(hook_node)) {
        catalyst::logger.debug("[Catalyst Hook: {}] Running command: {}", hook_name, *command);
        std::vector<std::string> cmd = catalyst::hooks::shellCmd(*command);
        if (auto res = catalyst::processExec(std::move(cmd), std::nullopt, environment); !res || res->get()) {
            return std::unexpected(std::format("Hook '{}' command failed: {}", hook_name, *command));
        }
    }

    catalyst::logger.debug("Hook finished successfully: {}", hook_name);
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
