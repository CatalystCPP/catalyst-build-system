#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::hooks {
std::expected<void, std::string> executeCommandHook(const YAML::Node &item, const std::string &hook_name) {
    auto command = item["command"].as<std::string>();
    catalyst::logger.debug("[Catalyst Hook: {}] Running command: {}", hook_name, command);
    if (catalyst::processExec(shellCmd(command)).value().get() != 0) {
        return std::unexpected(std::format("Hook '{}' command failed: {}", hook_name, command));
    }
    return {};
}
} // namespace catalyst::hooks
