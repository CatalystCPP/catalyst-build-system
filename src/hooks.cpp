#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <catalyst/hooks.hpp>

#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

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

std::expected<void, std::string> executeHook(ryml::ConstNodeRef profile_comp, std::string_view hook_name) {
    using catalyst::utils::yaml::asString;
    using catalyst::utils::yaml::child;

    catalyst::logger.debug("Executing hook: {}", hook_name);
    ryml::ConstNodeRef hook_node = child(child(profile_comp, "hooks"), hook_name);
    if (!hook_node.readable()) {
        catalyst::logger.debug("No hook defined for: {}", hook_name);
        return {};
    }

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
            std::expected<void, std::string> res;
            if (child(item, "command").readable()) {
                res = catalyst::hooks::executeCommandHook(item, hook_name);
            } else if (child(item, "script").readable()) {
                res = catalyst::hooks::executeScriptHook(item, hook_name);
            } else if (child(item, "catalyst").readable()) {
                res = catalyst::hooks::executeCatalystHook(item, hook_name);
            } else if (ryml::ConstNodeRef codegen = child(item, "codegen"); codegen.readable()) {
                res = catalyst::hooks::executeCodegenHook(codegen, hook_name);
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
        if (auto res = catalyst::processExec(std::move(cmd)); !res || res->get()) {
            return std::unexpected(std::format("Hook '{}' command failed: {}", hook_name, *command));
        }
    }

    catalyst::logger.debug("Hook finished successfully: {}", hook_name);
    return {};
}

} // namespace

namespace catalyst::hooks {

std::expected<void, std::string> preBuild(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "pre-build");
}

std::expected<void, std::string> postBuild(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "post-build");
}

std::expected<void, std::string> onBuildFailure(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "on-build-failure");
}

std::expected<void, std::string> preGenerate(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "pre-generate");
}

std::expected<void, std::string> postGenerate(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "post-generate");
}

std::expected<void, std::string> preFetch(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "pre-fetch");
}

std::expected<void, std::string> postFetch(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "post-fetch");
}

std::expected<void, std::string> preClean(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "pre-clean");
}

std::expected<void, std::string> postClean(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "post-clean");
}

std::expected<void, std::string> preRun(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "pre-run");
}

std::expected<void, std::string> postRun(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "post-run");
}

std::expected<void, std::string> preTest(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "pre-test");
}

std::expected<void, std::string> postTest(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "post-test");
}

std::expected<void, std::string> preBench(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "pre-bench");
}

std::expected<void, std::string> postBench(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "post-bench");
}

std::expected<void, std::string> prePack(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "pre-pack");
}

std::expected<void, std::string> postPack(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.rootRef(), "post-pack");
}
} // namespace catalyst::hooks
