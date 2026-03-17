#include <expected>
#include <format>
#include <string>
#include <vector>

#include <catalyst/hooks.hpp>
#include <yaml-cpp/yaml.h>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/node/node.h"

namespace catalyst::hooks {

namespace {
std::expected<void, std::string> executeHook(const YAML::Node &profile_comp, const std::string &hook_name) {
    catalyst::logger.log(LogLevel::DEBUG, "Executing hook: {}", hook_name);
    if (!profile_comp["hooks"] || !profile_comp["hooks"][hook_name]) {
        catalyst::logger.log(LogLevel::DEBUG, "No hook defined for: {}", hook_name);
        return {}; // No hook defined, so we do nothing.
    }

    auto shell_cmd = [](const std::string &cmd) -> std::vector<std::string> {
#if defined(_WIN32)
        return {"cmd", "/c", cmd};
#else
        return {"/bin/sh", "-c", cmd};
#endif
    };

    const YAML::Node &hook_node = profile_comp["hooks"][hook_name];
    if (hook_node.IsSequence()) {
        for (const auto &item : hook_node) {
            if (item["command"]) {
                auto command = item["command"].as<std::string>();
                catalyst::logger.log(LogLevel::DEBUG, "[Catalyst Hook: {}] Running command: {}", hook_name, command);
                if (catalyst::processExec(shell_cmd(command)).value().get() != 0) {
                    return std::unexpected(std::format("Hook '{}' command failed: {}", hook_name, command));
                }
            } else if (item["script"]) {
                auto script = item["script"].as<std::string>();
                catalyst::logger.log(LogLevel::DEBUG, "[Catalyst Hook: {}] Running script: {}", hook_name, script);
                if (catalyst::processExec(shell_cmd(script)).value().get() != 0) {
                    return std::unexpected(std::format("Hook '{}' script failed: {}", hook_name, script));
                }
            }
        }
    } else if (hook_node.IsScalar()) {
        auto command = hook_node.as<std::string>();
        catalyst::logger.log(LogLevel::DEBUG, "[Catalyst Hook: {}] Running command: {}", hook_name, command);
        if (catalyst::processExec(shell_cmd(command)).value().get() != 0) {
            return std::unexpected("Hook '" + hook_name + "' command failed: " + command);
        }
    }

    catalyst::logger.log(LogLevel::DEBUG, "Hook finished successfully: {}", hook_name);
    return {};
}
} // namespace

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
