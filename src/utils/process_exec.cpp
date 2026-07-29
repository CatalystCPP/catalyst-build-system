#include "catalyst/process_exec.hpp"

#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <reproc++/run.hpp>

#include "catalyst/cob_embedded.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/toolchain.hpp"

#include "reproc++/drain.hpp"
#include "reproc++/reproc.hpp"

namespace catalyst {

namespace fs = std::filesystem;

namespace {
fs::path getEmbeddedCobPath() {
    const char *home = std::getenv(
#ifdef _WIN32
        "USERPROFILE"
#else
        "HOME"
#endif
    );
    fs::path base_dir;
    if (home) {
        base_dir = fs::path(home) / ".catalyst" / "bin";
    } else {
        base_dir = fs::temp_directory_path() / "catalyst" / "bin";
    }

    fs::path cob_name = "cob";
#ifdef _WIN32
    catalyst::toolchain::ToolchainDef tc;
    std::string exe_ext = tc.extensions.executable.empty() ? ".exe" : tc.extensions.executable;
    cob_name.replace_extension(exe_ext);
#endif
    return base_dir / cob_name;
}

Result<void> ensureEmbeddedCobExtracted() {
    fs::path cob_path = getEmbeddedCobPath();
    std::error_code ec;

    if (fs::exists(cob_path, ec)) {
        if (fs::file_size(cob_path, ec) == catalyst::embedded::cob_binary_len) {
            return {};
        }
        fs::remove(cob_path, ec);
        if (ec) {
            return std::unexpected("Failed to replace stale embedded cob: " + ec.message());
        }
    }

    fs::create_directories(cob_path.parent_path(), ec);
    if (ec) {
        return std::unexpected("Failed to create directories for embedded cob: " + ec.message());
    }

    std::random_device random;
    fs::path temporary_path = cob_path;
    temporary_path += std::format(".tmp.{:08x}{:08x}", random(), random());

    std::ofstream out(temporary_path, std::ios::binary);
    if (!out) {
        return std::unexpected("Failed to open embedded cob destination file for writing: " + temporary_path.string());
    }

    out.write(reinterpret_cast<const char *>(catalyst::embedded::cob_binary), catalyst::embedded::cob_binary_len);
    out.close();
    if (!out) {
        fs::remove(temporary_path, ec);
        return std::unexpected("Failed to write embedded cob destination: " + temporary_path.string());
    }

#ifndef _WIN32
    fs::permissions(temporary_path,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read
                        | fs::perms::others_exec,
                    fs::perm_options::replace,
                    ec);
    if (ec) {
        fs::remove(temporary_path, ec);
        return std::unexpected("Failed to set executable permissions on embedded cob: " + ec.message());
    }
#endif

    fs::rename(temporary_path, cob_path, ec);
    if (ec) {
        std::error_code inspect_ec;
        const bool another_process_completed =
            fs::exists(cob_path, inspect_ec)
            && fs::file_size(cob_path, inspect_ec) == catalyst::embedded::cob_binary_len;
        fs::remove(temporary_path, inspect_ec);
        if (!another_process_completed) {
            return std::unexpected("Failed to install embedded cob: " + ec.message());
        }
    }

    return {};
}
} // namespace

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

Result<std::future<int>> ProcessExecutor::processExec(std::vector<std::string> &&args,
                                                      std::optional<std::string> working_dir,
                                                      std::optional<std::unordered_map<std::string, std::string>> env,
                                                      bool silent) {
    if (args.empty()) {
        return std::unexpected("Cannot execute empty command");
    }

    std::vector<std::string> local_args = std::move(args);
    if (local_args[0] == "cob") {
        if (auto res = ensureEmbeddedCobExtracted(); !res) {
            return std::unexpected(res.error());
        }
        local_args[0] = getEmbeddedCobPath().string();
    }

    return std::async(
        std::launch::async,
        [args = std::move(local_args), working_dir = std::move(working_dir), env = std::move(env), silent]() -> int {
            reproc::options options;
            if (silent) {
                options.redirect.out.type = reproc::redirect::discard;
                options.redirect.err.type = reproc::redirect::discard;
            } else {
                options.redirect.out.type = reproc::redirect::parent;
                options.redirect.err.type = reproc::redirect::parent;
            }

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

Result<std::string>
ProcessExecutor::processExecStdout(const std::vector<std::string> &args,
                                   const std::optional<std::string> &working_dir,
                                   const std::optional<std::unordered_map<std::string, std::string>> &env) {
    if (args.empty()) {
        return std::unexpected("Cannot execute empty command");
    }

    std::vector<std::string> local_args = args;
    if (local_args[0] == "cob") {
        if (auto res = ensureEmbeddedCobExtracted(); !res) {
            return std::unexpected(res.error());
        }
        local_args[0] = getEmbeddedCobPath().string();
    }

    reproc::options options;

    std::vector<std::string> env_strings;
    std::vector<const char *> env_ptrs;
    configure_opt::workingDir(working_dir, options);
    configure_opt::env(env, options, env_strings, env_ptrs);

    std::string output;
    reproc::sink::string sink(output);
    auto [status, ec] = reproc::run(local_args, options, sink, reproc::sink::null);

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

Result<std::future<int>> processExec(std::vector<std::string> &&args,
                                     std::optional<std::string> working_dir,
                                     std::optional<std::unordered_map<std::string, std::string>> env,
                                     bool silent) {
    return getProcessExecutor().processExec(std::move(args), std::move(working_dir), std::move(env), silent);
}

Result<std::string> processExecStdout(const std::vector<std::string> &args,
                                      const std::optional<std::string> &working_dir,
                                      const std::optional<std::unordered_map<std::string, std::string>> &env) {
    return getProcessExecutor().processExecStdout(args, working_dir, env);
}
} // namespace catalyst
