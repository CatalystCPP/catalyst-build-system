#pragma once
#include <expected>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace catalyst {
std::expected<std::future<int>, std::string>
processExec(std::vector<std::string> &&args,
            std::optional<std::string> working_dir = std::nullopt,
            std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt);

std::expected<std::string, std::string>
processExecStdout(const std::vector<std::string> &args,
                  const std::optional<std::string> &working_dir = std::nullopt,
                  const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt);
} // namespace catalyst
