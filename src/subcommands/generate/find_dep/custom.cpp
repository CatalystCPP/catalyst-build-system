#include <cstdlib>
#include <expected>
#include <format>
#include <string>
#include <unordered_map>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::generate {

Result<FindRes> findCustom(ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc) {
    namespace yaml = catalyst::utils::yaml;
    std::string dep_name = yaml::asString(yaml::child(dep, "name")).value_or("<unnamed>");
    catalyst::logger.debug("Resolving custom dependency: {}", dep_name);

    auto command = yaml::asString(yaml::child(dep, "command"));
    auto script = yaml::asString(yaml::child(dep, "script"));
    if (!command && !script) {
        return std::unexpected(
            std::format("dependency '{}' has source: custom but defines neither 'command' nor 'script'.", dep_name));
    }
    if (command && script) {
        return std::unexpected(std::format(
            "dependency '{}' has source: custom but defines both 'command' and 'script'; only one is allowed.",
            dep_name));
    }
    const std::string &invocation = command ? *command : *script;

    std::unordered_map<std::string, std::string> env;
    if (const char *path_env = std::getenv("PATH")) {
        env["PATH"] = path_env;
    }
    env["CATALYST_DEP_NAME"] = dep_name;
    if (auto version = yaml::asString(yaml::child(dep, "version"))) {
        env["CATALYST_DEP_VERSION"] = *version;
    }

    catalyst::logger.debug("[custom source: {}] Running: {}", dep_name, invocation);
    auto stdout_res = catalyst::processExecStdout(catalyst::hooks::shellCmd(invocation), std::nullopt, env);
    if (!stdout_res) {
        return std::unexpected(
            std::format("custom source for dependency '{}' failed to run: {}", dep_name, stdout_res.error()));
    }
    if (stdout_res->find_first_not_of(" \t\r\n") == std::string::npos) {
        return std::unexpected(
            std::format("custom source for dependency '{}' produced no output on stdout.", dep_name));
    }

    FindRes result{};
    appendPkgConfigFlags(*stdout_res, tc, PkgConfigFlagBucket::Link, result);
    if (result.inc_path.empty() && result.lib_path.empty() && result.libs.empty()) {
        return std::unexpected(std::format(
            "custom source for dependency '{}' produced output with no recognizable -I/-L/-l flags: '{}'",
            dep_name,
            *stdout_res));
    }

    catalyst::logger.debug("Resolved custom dependency '{}': cflags='{}' L='{}' l='{}'",
                           dep_name,
                           result.inc_path,
                           result.lib_path,
                           result.libs);
    return result;
}

} // namespace catalyst::generate
