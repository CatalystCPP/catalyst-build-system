#include <string_view>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

#include <chrono>
#include <filesystem>

namespace catalyst::hooks {
Result<void>
executeScriptHook(ryml::ConstNodeRef item, std::string_view hook_name, const HookEnvironment &environment) {
    auto script = utils::yaml::asString(utils::yaml::child(item, "script"));
    if (!script)
        return std::unexpected(std::format("Hook '{}' script is not a string", hook_name));
    catalyst::logger.explain("Executing script hook for '{}': '{}'", hook_name, *script);
    catalyst::logger.debug("[Catalyst Hook: {}] Running script: {}", hook_name, *script);
    auto start_time = std::chrono::steady_clock::now();
    auto res = catalyst::processExec(shellCmd(*script), std::nullopt, environment);
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
    if (!res) {
        catalyst::logger.explain("Hook '{}' script execution failed: {}", hook_name, res.error());
        return std::unexpected(std::format("Hook '{}' script execution failed: {}", hook_name, res.error()));
    }
    if (res->get()) {
        catalyst::logger.explain("Hook '{}' script failed with exit code {} (duration: {} ms)", hook_name,
                                 res->get(), elapsed_ms);
        return std::unexpected(std::format("Hook '{}' script failed: {}", hook_name, *script));
    }
    catalyst::logger.explain("Hook '{}' script succeeded in {} ms (exit code: 0)", hook_name, elapsed_ms);
    return {};
}
} // namespace catalyst::hooks
