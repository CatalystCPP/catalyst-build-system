#include <string_view>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::hooks {
Result<void> executeCommandHook(ryml::ConstNodeRef item, std::string_view hook_name) {
    auto command = utils::yaml::asString(utils::yaml::child(item, "command"));
    if (!command)
        return std::unexpected(std::format("Hook '{}' command is not a string", hook_name));
    catalyst::logger.debug("[Catalyst Hook: {}] Running command: {}", hook_name, *command);
    auto res = catalyst::processExec(shellCmd(*command));
    if (!res)
        return std::unexpected(std::format("Hook '{}' command execution failed: {}", hook_name, res.error()));
    if (res->get())
        return std::unexpected(std::format("Hook '{}' command failed: {}", hook_name, *command));
    return {};
}
} // namespace catalyst::hooks
