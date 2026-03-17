#include <cctype>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <tuple>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "catalyst/hooks.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/subcommands/test.hpp"

namespace fs = std::filesystem;

namespace catalyst::test {

namespace {
std::string commandStr(const fs::path &executable, const std::vector<std::string> &params) {
    catalyst::logger.log(LogLevel::DEBUG, "Constructing command string.");
    std::string command = executable;
    for (const auto &param : params) {
        command += " " + param;
    }
    return command;
}
} // namespace

std::expected<void, std::string> action(const Parse &args) {
    catalyst::logger.log(LogLevel::DEBUG, "Test subcommand invoked.");

    if (args.workspace) {
        fs::path current = fs::current_path();
        bool is_root = false;
        try {
            is_root = fs::equivalent(args.workspace->getRoot(), current);
        } catch (...) {
            std::ignore;
        }

        if (is_root) {
            catalyst::logger.log(LogLevel::INFO, "Running tests for all workspace members.");
            bool any_failed = false;
            std::string failed_members;

            for (const auto &[name, member] : args.workspace->getMembers()) {
                catalyst::logger.log(LogLevel::INFO, "Testing member: {}", name);
                fs::current_path(member.path);

                Parse member_args = args;
                member_args.workspace = std::nullopt; // Prevent recursion loop

                if (auto res = action(member_args); !res) {
                    catalyst::logger.log(LogLevel::ERROR, "Test failed for member: {}", name);
                    any_failed = true;
                    failed_members += name + " ";
                }
                fs::current_path(current);
            }
            if (any_failed) {
                return std::unexpected("Tests failed for members: " + failed_members);
            }
            return {};
        }
    }

    std::vector<std::string> profiles{"common", "test"};

    catalyst::logger.log(LogLevel::DEBUG, "Composing profiles.");
    YAML::Node profile_comp;
    auto res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(res.error());
    }
    profile_comp = res.value();

    catalyst::logger.log(LogLevel::DEBUG, "Running pre-test hooks.");
    if (auto res = hooks::preTest(profile_comp); !res) {
        return res;
    }

    std::string exe;
    std::string build_dir;
    auto str_to_lower = [](std::string &input) -> void {
        auto lower = [](const char c) -> char { return static_cast<char>(std::tolower(c)); };
        std::transform(input.begin(), input.end(), input.begin(), lower);
        return;
    };
    std::string target_type = profile_comp["manifest"]["type"].Scalar();
    str_to_lower(target_type);

    if (!profile_comp["manifest"]["type"].IsDefined() || target_type != "binary") {
        return std::unexpected("profile does not build a binary target");
    }

    if (!profile_comp["manifest"]["dirs"]["build"]) {
        return std::unexpected("build directory is not defined");
    }
    build_dir = profile_comp["manifest"]["dirs"]["build"].as<std::string>();

    if (profile_comp["manifest"]["provides"] && profile_comp["manifest"]["provides"].as<std::string>() != "") {
        exe = profile_comp["manifest"]["provides"].as<std::string>();
    } else if (profile_comp["manifest"]["name"] && profile_comp["manifest"]["name"].as<std::string>() != "") {
        exe = profile_comp["manifest"]["name"].as<std::string>();
    } else {
        return std::unexpected("unable to figure out executable name. "
                               "manifest.name and manifest.provides are undefined");
    }

    fs::path exe_path = fs::absolute(fs::path(std::format("{}/{}", build_dir, exe)));
    std::string command = commandStr(exe_path, args.params);
    catalyst::logger.log(LogLevel::DEBUG, "Executing command: {}", command);

    std::vector<std::string> exec_args;
    exec_args.push_back(exe_path.string());
    exec_args.insert(exec_args.end(), args.params.begin(), args.params.end());

    if (int res = catalyst::processExec(std::move(exec_args)).value().get(); res) {
        return std::unexpected(
            std::format("Target executable: {} exited with failure code: {}", exe_path.string(), res));
    }

    catalyst::logger.log(LogLevel::DEBUG, "Running post-test hooks.");
    if (auto res = hooks::postTest(profile_comp); !res) {
        return res;
    }

    catalyst::logger.log(LogLevel::DEBUG, "Test subcommand finished successfully.");
    return {};
}

} // namespace catalyst::test
