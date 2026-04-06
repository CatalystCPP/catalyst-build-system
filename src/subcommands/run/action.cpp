#include <algorithm>
#include <cctype>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/subcommands/run.hpp"
#include "catalyst/utils/log/log.hpp"
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

std::expected<void, std::string> action(const Parse &args) {
    catalyst::logger.debug("Run subcommand invoked.");
    std::vector<std::string> profiles;
    if (args.profile != "common") {
        profiles.emplace_back("common");
    }
    profiles.push_back(args.profile);

    catalyst::logger.debug("Composing profiles.");
    YAML::Node profile_comp;
    std::expected<YAML::Node, std::string> res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(res.error());
    }
    profile_comp = res.value();

    catalyst::logger.debug("Running pre-run hooks.");
    if (std::expected<void, std::string> res = hooks::preRun(profile_comp); !res) {
        return res;
    }

    std::string exe;
    std::string build_dir;
    auto str_to_lower = [](std::string &input) -> void {
        auto lower = [](const char c) -> char { return static_cast<char>(std::tolower(c)); };
        std::ranges::transform(input, input.begin(), lower);
        return;
    };
    std::string target_type = profile_comp["manifest"]["type"].Scalar();
    str_to_lower(target_type);

    if (!profile_comp["manifest"]["type"].IsDefined() || target_type != "binary") {
        if (!profile_comp["manifest"]["type"].IsDefined()) {
            return std::unexpected(std::format("Profile: {} does not define field: 'manifest.type'.", args.profile));
        }
        return std::unexpected(std::format(
            "Profile: {} defines 'manifest.type' = {}. Expected 'manifest.type' = BINARY", args.profile, target_type));
    }

    std::string build_dir_base = "build";
    if (profile_comp["manifest"]["dirs"]["build"]) {
        build_dir_base = profile_comp["manifest"]["dirs"]["build"].as<std::string>();
        if (build_dir_base.empty())
            build_dir_base = "build";
    }
    build_dir = catalyst::utils::yaml::multiplexedBuildDir(build_dir_base, profiles).string();

    if (profile_comp["manifest"]["provides"] && profile_comp["manifest"]["provides"].as<std::string>() != "") {
        exe = profile_comp["manifest"]["provides"].as<std::string>();
    } else if (profile_comp["manifest"]["name"] && profile_comp["manifest"]["name"].as<std::string>() != "") {
        exe = profile_comp["manifest"]["name"].as<std::string>();
    } else {
        return std::unexpected("Unable to figure out executable name."
                               "manifest.name and manifest.provides are undefined");
    }

    fs::path exe_path = fs::absolute(fs::path(std::format("{}/{}", build_dir, exe)));
    std::string command = commandStr(exe_path, args.params);
    catalyst::toolchain::ToolchainDef tc;
    std::expected<std::string, std::string> lib_path_res = catalyst::generate::libPath(profile_comp, profiles, tc);
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
    exec_args.insert(exec_args.end(), args.params.begin(), args.params.end());

    if (int res = catalyst::processExec(std::move(exec_args), std::nullopt, exec_env).value().get(); res) {
        return std::unexpected(
            std::format("Target executable: {} exited with failure code: {}", exe_path.string(), res));
    }

    catalyst::logger.debug("Running post-run hooks.");
    if (std::expected<void, std::string> res = hooks::postRun(profile_comp); !res) {
        return res;
    }

    catalyst::logger.debug("Run subcommand finished successfully.");
    return {};
}

} // namespace catalyst::run
