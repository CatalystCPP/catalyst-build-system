#include "catalyst/utils/runtime_context.hpp"

#include <span>
#include <string>
#include <vector>

namespace catalyst::utils::runtime {
namespace {

// CLI dispatch is process-global. These values are set before an action starts
// and remain stable while that action constructs hook configurations.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<std::string> g_command_line;
std::vector<std::string> g_enabled_features;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace

void setCommandLine(std::span<const std::string> command_line) {
    g_command_line.assign(command_line.begin(), command_line.end());
}

const std::vector<std::string> &commandLine() {
    return g_command_line;
}

void setEnabledFeatures(std::span<const std::string> features) {
    g_enabled_features.assign(features.begin(), features.end());
}

const std::vector<std::string> &enabledFeatures() {
    return g_enabled_features;
}

} // namespace catalyst::utils::runtime
