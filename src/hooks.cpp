#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include <catalyst/hooks.hpp>
#include <yaml-cpp/yaml.h>

#include "catalyst/dispatch.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/node/node.h"

namespace {

std::vector<std::string> shellCmd(const std::string &cmd) {
#if defined(_WIN32)
    return {"cmd", "/c", cmd};
#else
    return {"/bin/sh", "-c", cmd};
#endif
}

std::expected<void, std::string> executeCommandHook(const YAML::Node &item, const std::string &hook_name) {
    auto command = item["command"].as<std::string>();
    catalyst::logger.debug("[Catalyst Hook: {}] Running command: {}", hook_name, command);
    if (catalyst::processExec(shellCmd(command)).value().get() != 0) {
        return std::unexpected(std::format("Hook '{}' command failed: {}", hook_name, command));
    }
    return {};
}

std::expected<void, std::string> executeScriptHook(const YAML::Node &item, const std::string &hook_name) {
    auto script = item["script"].as<std::string>();
    catalyst::logger.debug("[Catalyst Hook: {}] Running script: {}", hook_name, script);
    if (catalyst::processExec(shellCmd(script)).value().get() != 0) {
        return std::unexpected(std::format("Hook '{}' script failed: {}", hook_name, script));
    }
    return {};
}

std::expected<void, std::string> executeCatalystHook(const YAML::Node &item, const std::string &hook_name) {
    auto cat_node = item["catalyst"];
    if (cat_node.IsScalar()) {
        auto args = cat_node.as<std::string>();
        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, args);
        auto res = catalyst::dispatchHook("catalyst " + args);
        if (!res) {
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
        }
    } else if (cat_node.IsMap()) {
        if (!cat_node["subcommand"]) {
            return std::unexpected(
                std::format("Hook '{}' catalyst missing required 'subcommand' field", hook_name));
        }
        std::vector<std::string> args;
        if (cat_node["global_args"]) {
            for (const auto &a : cat_node["global_args"])
                args.push_back(a.as<std::string>());
        }
        args.push_back(cat_node["subcommand"].as<std::string>());
        if (cat_node["profiles"]) {
            for (const auto &p : cat_node["profiles"]) {
                args.push_back("-p");
                args.push_back(p.as<std::string>());
            }
        }
        if (cat_node["args"]) {
            for (const auto &a : cat_node["args"])
                args.push_back(a.as<std::string>());
        }

        std::string joined;
        joined.reserve(128);
        for (const auto &a : args)
            joined += a + " ";
        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, joined);

        auto res = catalyst::dispatchHook(args);
        if (!res) {
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
        }
    } else {
        return std::unexpected(std::format("Hook '{}' catalyst has invalid type", hook_name));
    }
    return {};
}

std::expected<void, std::string> executeCodegenHook(const YAML::Node &codegen_node,
                                                      const std::string &hook_name) {
    if (!codegen_node["cmd"]) {
        return std::unexpected(std::format("Hook '{}' codegen missing required 'cmd' field", hook_name));
    }
    auto cmd = codegen_node["cmd"].as<std::string>();

    std::vector<std::string> inputs;
    if (codegen_node["input"]) {
        if (codegen_node["input"].IsSequence()) {
            for (const auto &in : codegen_node["input"])
                inputs.push_back(in.as<std::string>());
        } else if (codegen_node["input"].IsScalar()) {
            inputs.push_back(codegen_node["input"].as<std::string>());
        }
    }

    std::vector<std::string> outputs;
    if (codegen_node["output"]) {
        if (codegen_node["output"].IsSequence()) {
            for (const auto &out : codegen_node["output"])
                outputs.push_back(out.as<std::string>());
        } else if (codegen_node["output"].IsScalar()) {
            outputs.push_back(codegen_node["output"].as<std::string>());
        }
    }

    // Substitute $IN[i]/$OUT[i] (indexed) and $IN/$OUT (all, space-joined) in cmd.
    // Process indexed forms first to avoid $IN matching inside $IN[0].
    auto replace_all = [](std::string &s, const std::string &from, const std::string &to) {
        std::string::size_type pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    if (cmd.contains("$IN[")) {
        for (size_t i = 0; i < inputs.size(); ++i)
            replace_all(cmd, "$IN[" + std::to_string(i) + "]", inputs[i]);
    }
    if (cmd.contains("$OUT[")) {
        for (size_t i = 0; i < outputs.size(); ++i)
            replace_all(cmd, "$OUT[" + std::to_string(i) + "]", outputs[i]);
    }

    if (cmd.contains("$IN[")) {
        return std::unexpected(std::format("Hook '{}' codegen cmd references out-of-bounds $IN index", hook_name));
    }
    if (cmd.contains("$OUT[")) {
        return std::unexpected(std::format("Hook '{}' codegen cmd references out-of-bounds $OUT index", hook_name));
    }

    // Replace bare $IN/$OUT with space-joined lists
    auto join_paths = [](const std::vector<std::string> &paths) {
        std::string result;
        for (size_t i = 0; i < paths.size(); ++i) {
            if (i > 0)
                result += ' ';
            result += paths[i];
        }
        return result;
    };
    replace_all(cmd, "$IN", join_paths(inputs));
    replace_all(cmd, "$OUT", join_paths(outputs));

    bool should_run = true;
    if (!inputs.empty() && !outputs.empty()) {
        should_run = false;
        std::filesystem::file_time_type oldest_out = std::filesystem::file_time_type::max();
        for (const auto &out : outputs) {
            if (!std::filesystem::exists(out)) {
                should_run = true;
                break;
            }
            auto mtime = std::filesystem::last_write_time(out);
            if (mtime < oldest_out)
                oldest_out = mtime;
        }

        if (!should_run) {
            for (const auto &in : inputs) {
                if (!std::filesystem::exists(in)) {
                    catalyst::logger.warn("[Catalyst Hook: {}] codegen input file not found: {}", hook_name, in);
                    should_run = true;
                    break;
                }
                auto mtime = std::filesystem::last_write_time(in);
                if (mtime > oldest_out) {
                    should_run = true;
                    break;
                }
            }
        }
    }

    if (should_run) {
        catalyst::logger.debug("[Catalyst Hook: {}] Running codegen: {}", hook_name, cmd);
        if (catalyst::processExec(shellCmd(cmd)).value().get() != 0) {
            return std::unexpected(std::format("Hook '{}' codegen failed: {}", hook_name, cmd));
        }
    } else {
        catalyst::logger.debug("[Catalyst Hook: {}] Skipping codegen: {} (up-to-date)", hook_name, cmd);
    }
    return {};
}

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
                res = executeCommandHook(item, hook_name);
            } else if (item["script"]) {
                res = executeScriptHook(item, hook_name);
            } else if (item["catalyst"]) {
                res = executeCatalystHook(item, hook_name);
            } else if (item["codegen"]) {
                res = executeCodegenHook(item["codegen"], hook_name);
            }
            if (!res) {
                return res;
            }
        }
    } else if (hook_node.IsScalar()) {
        auto command = hook_node.as<std::string>();
        catalyst::logger.debug("[Catalyst Hook: {}] Running command: {}", hook_name, command);
        if (catalyst::processExec(shellCmd(command)).value().get() != 0) {
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
