#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::hooks {
std::expected<void, std::string> executeScriptHook(const YAML::Node &item, const std::string &hook_name) {
    auto script = item["script"].as<std::string>();
    catalyst::logger.debug("[Catalyst Hook: {}] Running script: {}", hook_name, script);
    if (catalyst::processExec(shellCmd(script)).value().get() != 0) {
        return std::unexpected(std::format("Hook '{}' script failed: {}", hook_name, script));
    }
    return {};
}
} // namespace catalyst::hooks
