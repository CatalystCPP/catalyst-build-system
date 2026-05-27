#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::hooks {
std::expected<void, std::string> executeCommandHook(const YAML::Node &item, const std::string &hook_name) {
    auto command = item["command"].as<std::string>();
    catalyst::logger.debug("[Catalyst Hook: {}] Running command: {}", hook_name, command);
    if (auto res = catalyst::processExec(shellCmd(command)); !res)
        return std::unexpected(std::format("Hook '{}' command execution failed: {}", hook_name, res.error()));
    else if (res->get())
        return std::unexpected(std::format("Hook '{}' command failed: {}", hook_name, command));
    return {};
}
} // namespace catalyst::hooks
