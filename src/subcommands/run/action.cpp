#include <algorithm>
#include <cctype>
#include <expected>
#include <filesystem>
#include <format>
#include <print>
#include <string>
#include <vector>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/subcommands/run.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace fs = std::filesystem;

namespace catalyst::run {

namespace {
std::string commandStr(const fs::path &executable, const std::vector<std::string> &params) {
    catalyst::logger.debug("Constructing command string.");
    std::string command = executable;
    for (const auto &param : params) {
        command += " " + param;
    }
    return command;
}
} // namespace

Result<void> action(const Parse &args) {
    catalyst::logger.debug("Run subcommand invoked.");
    std::vector<std::string> profiles;
    if (!std::ranges::contains(args.profiles, "common")) {
        profiles.emplace_back("common");
        profiles.insert(profiles.end(), args.profiles.begin(), args.profiles.end());
    } else {
        profiles = args.profiles;
    }

    catalyst::logger.debug("Composing profiles.");
    auto res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(res.error());
    }
    const utils::yaml::Configuration &profile_comp = res.value();

    catalyst::logger.debug("Running pre-run hooks.");
    if (Result<void> hook_res = hooks::preRun(profile_comp); !hook_res) {
        return hook_res;
    }

    std::string exe;
    std::string build_dir;
    auto str_to_lower = [](std::string &input) -> void {
        auto lower = [](const char c) -> char { return static_cast<char>(std::tolower(c)); };
        std::ranges::transform(input, input.begin(), lower);
        return;
    };
    std::optional<std::string> type_opt = profile_comp.getString("manifest.type");
    std::string target_type = type_opt.value_or("");
    str_to_lower(target_type);

    if (!type_opt || target_type != "binary") {
        if (!type_opt) {
            return std::unexpected("Profile does not define field: 'manifest.type'.");
        }
        return std::unexpected(
            std::format("Profile defines 'manifest.type' = {}. Expected 'manifest.type' = BINARY", target_type));
    }

    std::string build_dir_base = profile_comp.getString("manifest.dirs.build").value_or("build");
    if (build_dir_base.empty())
        build_dir_base = "build";
    build_dir = catalyst::utils::yaml::multiplexedBuildDir(build_dir_base, profiles).string();

    if (auto provides = profile_comp.getString("manifest.provides"); provides && !provides->empty()) {
        exe = *provides;
    } else if (auto name = profile_comp.getString("manifest.name"); name && !name->empty()) {
        exe = *name;
    } else {
        return std::unexpected("Unable to figure out executable name."
                               "manifest.name and manifest.provides are undefined");
    }

    fs::path exe_path = fs::absolute(fs::path(std::format("{}/{}", build_dir, exe)));
    std::string command = commandStr(exe_path, args.params);
    catalyst::toolchain::ToolchainDef tc;
    if (auto tc_path = profile_comp.getString("manifest.toolchain")) {
        auto parsed = catalyst::toolchain::parseToolchain(*tc_path);
        if (!parsed) {
            return std::unexpected(std::format("Failed to load toolchain {}: {}", *tc_path, parsed.error()));
        }
        tc = std::move(*parsed);
    }

    Result<std::string> lib_path_res = catalyst::generate::libPath(profile_comp, profiles, tc);
    if (!lib_path_res) {
        return std::unexpected("Failed to generate LD_LIBRARY_PATH");
    }

    std::unordered_map<std::string, std::string> exec_env;
#if defined(_WIN32)
    exec_env["PATH"] = lib_path_res.value();
#elif defined(__APPLE__)
    exec_env["DYLD_LIBRARY_PATH"] = lib_path_res.value();
#else
    exec_env["LD_LIBRARY_PATH"] = lib_path_res.value();
#endif

    catalyst::logger.debug("Executing command: {}", command);

    std::vector<std::string> exec_args;
    exec_args.push_back(exe_path.string());
    if (!args.params.empty())
        exec_args.insert(exec_args.end(), args.params.begin(), args.params.end());

    if (int res = catalyst::processExec(std::move(exec_args), std::nullopt, exec_env).value().get(); res) {
        return std::unexpected(
            std::format("Target executable: {} exited with failure code: {}", exe_path.string(), res));
    }

    catalyst::logger.debug("Running post-run hooks.");
    if (Result<void> res = hooks::postRun(profile_comp); !res) {
        return res;
    }

    catalyst::logger.debug("Run subcommand finished successfully.");
    return {};
}

} // namespace catalyst::run
