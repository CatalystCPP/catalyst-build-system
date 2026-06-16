#pragma once
#include <expected>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "catalyst/utils/result.hpp"

namespace catalyst {

class IProcessExecutor {
public:
    virtual ~IProcessExecutor() = default;

    virtual Result<std::future<int>>
    processExec(std::vector<std::string> &&args,
                std::optional<std::string> working_dir = std::nullopt,
                std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt,
                bool silent = false) = 0;

    virtual Result<std::string>
    processExecStdout(const std::vector<std::string> &args,
                      const std::optional<std::string> &working_dir = std::nullopt,
                      const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt) = 0;
};

class ProcessExecutor : public IProcessExecutor {
public:
    Result<std::future<int>> processExec(std::vector<std::string> &&args,
                                         std::optional<std::string> working_dir = std::nullopt,
                                         std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt,
                                         bool silent = false) override;

    Result<std::string>
    processExecStdout(const std::vector<std::string> &args,
                      const std::optional<std::string> &working_dir = std::nullopt,
                      const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt) override;
};

// Dependency injection accessors
IProcessExecutor &getProcessExecutor();
void setProcessExecutor(std::shared_ptr<IProcessExecutor> executor);

// Legacy free functions for backwards compatibility (optional, but good to keep until full refactor)
Result<std::future<int>> processExec(std::vector<std::string> &&args,
                                     std::optional<std::string> working_dir = std::nullopt,
                                     std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt,
                                     bool silent = false);

Result<std::string>
processExecStdout(const std::vector<std::string> &args,
                  const std::optional<std::string> &working_dir = std::nullopt,
                  const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt);

} // namespace catalyst
