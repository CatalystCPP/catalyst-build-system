#include <expected>
#include <filesystem>
#include <format>
#include <regex>
#include <string>
#include <vector>

#include <catalyst/hooks.hpp>
#include <yaml-cpp/yaml.h>

#include "catalyst/dispatch.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/node/node.h"

namespace catalyst::hooks {
std::vector<std::string> shellCmd(const std::string &cmd) {
#if defined(_WIN32)
    return {"cmd", "/c", cmd};
#else
    return {"/bin/sh", "-c", cmd};
#endif
}
} // namespace catalyst::hooks

namespace {

std::expected<void, std::string> executeHook(const YAML::Node &profile_comp, const std::string &hook_name) {
    catalyst::logger.debug("Executing hook: {}", hook_name);
    if (!profile_comp["hooks"] || !profile_comp["hooks"][hook_name]) {
        catalyst::logger.debug("No hook defined for: {}", hook_name);
        return {};
    }

    const YAML::Node &hook_node = profile_comp["hooks"][hook_name];

    // Normalize: if the hook is a bare map (not wrapped in a sequence), treat it as a single-element sequence.
    std::vector<YAML::Node> items;
    if (hook_node.IsSequence()) {
        for (const auto &item : hook_node)
            items.push_back(item);
    } else if (hook_node.IsMap()) {
        items.push_back(hook_node);
    }

    if (!items.empty()) {
        for (const auto &item : items) {
            std::expected<void, std::string> res;
            if (item["command"]) {
                res = catalyst::hooks::executeCommandHook(item, hook_name);
            } else if (item["script"]) {
                res = catalyst::hooks::executeScriptHook(item, hook_name);
            } else if (item["catalyst"]) {
                res = catalyst::hooks::executeCatalystHook(item, hook_name);
            } else if (item["codegen"]) {
                res = catalyst::hooks::executeCodegenHook(item["codegen"], hook_name);
            }
            if (!res) {
                return res;
            }
        }
    } else if (hook_node.IsScalar()) {
        auto command = hook_node.as<std::string>();
        catalyst::logger.debug("[Catalyst Hook: {}] Running command: {}", hook_name, command);
        std::vector<std::string> cmd = catalyst::hooks::shellCmd(command);
        if (auto res = catalyst::processExec(std::move(cmd)); !res || res->get()) {
            return std::unexpected("Hook '" + hook_name + "' command failed: " + command);
        }
    }

    catalyst::logger.debug("Hook finished successfully: {}", hook_name);
    return {};
}

} // namespace

namespace catalyst::hooks {

std::expected<void, std::string> preClean(const YAML::Node &profile_comp) {
    return executeHook(profile_comp, "pre-clean");
}

std::expected<void, std::string> postClean(const YAML::Node &profile_comp) {
    return executeHook(profile_comp, "post-clean");
}

std::expected<void, std::string> preRun(const YAML::Node &profile_comp) {
    return executeHook(profile_comp, "pre-run");
}

std::expected<void, std::string> postRun(const YAML::Node &profile_comp) {
    return executeHook(profile_comp, "post-run");
}

std::expected<void, std::string> preTest(const YAML::Node &profile_comp) {
    return executeHook(profile_comp, "pre-test");
}

std::expected<void, std::string> postTest(const YAML::Node &profile_comp) {
    return executeHook(profile_comp, "post-test");
}

std::expected<void, std::string> preBench(const YAML::Node &profile_comp) {
    return executeHook(profile_comp, "pre-bench");
}

std::expected<void, std::string> postBench(const YAML::Node &profile_comp) {
    return executeHook(profile_comp, "post-bench");
}

std::expected<void, std::string> preBuild(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "pre-build");
}

std::expected<void, std::string> postBuild(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "post-build");
}

std::expected<void, std::string> onBuildFailure(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "on-build-failure");
}

std::expected<void, std::string> preGenerate(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "pre-generate");
}

std::expected<void, std::string> postGenerate(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "post-generate");
}

std::expected<void, std::string> preFetch(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "pre-fetch");
}

std::expected<void, std::string> postFetch(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "post-fetch");
}

std::expected<void, std::string> preClean(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "pre-clean");
}

std::expected<void, std::string> postClean(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "post-clean");
}

std::expected<void, std::string> preRun(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "pre-run");
}

std::expected<void, std::string> postRun(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "post-run");
}

std::expected<void, std::string> preTest(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "pre-test");
}

std::expected<void, std::string> postTest(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "post-test");
}

std::expected<void, std::string> preBench(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "pre-bench");
}

std::expected<void, std::string> postBench(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "post-bench");
}

std::expected<void, std::string> preLink(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "pre-link");
}

std::expected<void, std::string> postLink(const utils::yaml::Configuration &profile_comp) {
    return executeHook(profile_comp.getRoot(), "post-link");
}

std::expected<void, std::string> onCompile([[maybe_unused]] const std::filesystem::path &file) {
    // The on_compile hook would need a different implementation,
    // as it's not tied to the main profile composition.
    // For now, we'll leave it as a no-op.
    return {};
}

} // namespace catalyst::hooks
