#include "catalyst/process_exec.hpp"

#include <expected>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <reproc++/run.hpp>

#include "reproc++/drain.hpp"
#include "reproc++/reproc.hpp"

namespace catalyst {

namespace configure_opt {
void env(const std::optional<std::unordered_map<std::string, std::string>> &env,
         reproc::options &options,
         std::vector<std::string> &env_strings,
         std::vector<const char *> &env_ptrs) {
    if (env) {
        options.env.behavior = reproc::env::extend;
        for (const auto &[key, value] : *env) {
            env_strings.push_back((key + "=").append(value));
        }
        for (const auto &s : env_strings) {
            env_ptrs.push_back(s.c_str());
        }
        env_ptrs.push_back(nullptr);
        options.env.extra = env_ptrs.data();
    }
}

void workingDir(const std::optional<std::string> &working_dir, reproc::options &options) {
    if (working_dir) {
        options.working_directory = working_dir->c_str();
    }
}
} // namespace configure_opt

std::expected<std::future<int>, std::string>
ProcessExecutor::processExec(std::vector<std::string> &&args,
                             std::optional<std::string> working_dir,
                             std::optional<std::unordered_map<std::string, std::string>> env) {
    if (args.empty()) {
        return std::unexpected("Cannot execute empty command");
    }

    return std::async(std::launch::async,
                      [args = std::move(args), working_dir = std::move(working_dir), env = std::move(env)]() -> int {
                          reproc::options options;
                          options.redirect.out.type = reproc::redirect::parent;
                          options.redirect.err.type = reproc::redirect::parent;

                          std::vector<std::string> env_strings;
                          std::vector<const char *> env_ptrs;
                          configure_opt::workingDir(working_dir, options);
                          configure_opt::env(env, options, env_strings, env_ptrs);

                          auto [status, ec] = reproc::run(args, options);

                          if (ec)
                              return -1;
                          return status;
                      });
}

std::expected<std::string, std::string>
ProcessExecutor::processExecStdout(const std::vector<std::string> &args,
                                   const std::optional<std::string> &working_dir,
                                   const std::optional<std::unordered_map<std::string, std::string>> &env) {
    if (args.empty()) {
        return std::unexpected("Cannot execute empty command");
    }

    reproc::options options;

    std::vector<std::string> env_strings;
    std::vector<const char *> env_ptrs;
    configure_opt::workingDir(working_dir, options);
    configure_opt::env(env, options, env_strings, env_ptrs);

    std::string output;
    reproc::sink::string sink(output);
    auto [status, ec] = reproc::run(args, options, sink, reproc::sink::null);

    if (ec) {
        return std::unexpected(ec.message());
    }
    return output;
}

namespace {
std::shared_ptr<IProcessExecutor> g_executor_owner = std::make_shared<ProcessExecutor>();
IProcessExecutor *g_executor = g_executor_owner.get();
} // namespace

IProcessExecutor &getProcessExecutor() {
    return *g_executor;
}

void setProcessExecutor(std::shared_ptr<IProcessExecutor> executor) {
    g_executor_owner = std::move(executor);
    g_executor = g_executor_owner.get();
}

std::expected<std::future<int>, std::string>
processExec(std::vector<std::string> &&args,
            std::optional<std::string> working_dir,
            std::optional<std::unordered_map<std::string, std::string>> env) {
    return getProcessExecutor().processExec(std::move(args), std::move(working_dir), std::move(env));
}

std::expected<std::string, std::string>
processExecStdout(const std::vector<std::string> &args,
                  const std::optional<std::string> &working_dir,
                  const std::optional<std::unordered_map<std::string, std::string>> &env) {
    return getProcessExecutor().processExecStdout(args, working_dir, env);
}
} // namespace catalyst
