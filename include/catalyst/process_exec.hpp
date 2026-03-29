#pragma once
#include <expected>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace catalyst {

class IProcessExecutor {
public:
    virtual ~IProcessExecutor() = default;

    virtual std::expected<std::future<int>, std::string>
    processExec(std::vector<std::string> &&args,
                std::optional<std::string> working_dir = std::nullopt,
                std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt) = 0;

    virtual std::expected<std::string, std::string>
    processExecStdout(const std::vector<std::string> &args,
                      const std::optional<std::string> &working_dir = std::nullopt,
                      const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt) = 0;
};

class ProcessExecutor : public IProcessExecutor {
public:
    std::expected<std::future<int>, std::string>
    processExec(std::vector<std::string> &&args,
                std::optional<std::string> working_dir = std::nullopt,
                std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt) override;

    std::expected<std::string, std::string>
    processExecStdout(const std::vector<std::string> &args,
                      const std::optional<std::string> &working_dir = std::nullopt,
                      const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt) override;
};

// Dependency injection accessors
IProcessExecutor &getProcessExecutor();
void setProcessExecutor(std::shared_ptr<IProcessExecutor> executor);

// Legacy free functions for backwards compatibility (optional, but good to keep until full refactor)
std::expected<std::future<int>, std::string>
processExec(std::vector<std::string> &&args,
            std::optional<std::string> working_dir = std::nullopt,
            std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt);

std::expected<std::string, std::string>
processExecStdout(const std::vector<std::string> &args,
                  const std::optional<std::string> &working_dir = std::nullopt,
                  const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt);

} // namespace catalyst
