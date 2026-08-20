#include <string_view>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::hooks {
Result<void>
executeScriptHook(ryml::ConstNodeRef item, std::string_view hook_name, const HookEnvironment &environment) {
    auto script = utils::yaml::asString(utils::yaml::child(item, "script"));
    if (!script)
        return std::unexpected(std::format("Hook '{}' script is not a string", hook_name));
    catalyst::logger.debug("[Catalyst Hook: {}] Running script: {}", hook_name, *script);
    auto res = catalyst::processExec(shellCmd(*script), std::nullopt, environment);
    if (!res)
        return std::unexpected(std::format("Hook '{}' script execution failed: {}", hook_name, res.error()));
    if (res->get())
        return std::unexpected(std::format("Hook '{}' script failed: {}", hook_name, *script));
    return {};
}
} // namespace catalyst::hooks
