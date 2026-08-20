#pragma once

#include <span>
#include <string>
#include <vector>

namespace catalyst::utils::runtime {

void setCommandLine(std::span<const std::string> command_line);
[[nodiscard]] const std::vector<std::string> &commandLine();

void setEnabledFeatures(std::span<const std::string> features);
[[nodiscard]] const std::vector<std::string> &enabledFeatures();

} // namespace catalyst::utils::runtime
