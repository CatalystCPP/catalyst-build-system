#include <algorithm>
#include <cctype>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <tuple>
#include <vector>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/subcommands/test.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace fs = std::filesystem;

namespace catalyst::test {

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
    catalyst::logger.debug("Test subcommand invoked.");

    if (args.workspace) {
        fs::path current = fs::current_path();
        bool is_root = false;
        try {
            is_root = fs::equivalent(args.workspace->getRoot(), current);
        } catch (...) {
            std::ignore;
        }

        if (is_root) {
            catalyst::logger.info("Running tests for all workspace members.");
            bool any_failed = false;
            std::string failed_members;

            for (const auto &[name, member] : args.workspace->getMembers()) {
                catalyst::logger.info("Testing member: {}", name);
                fs::current_path(member.path);

                Parse member_args = args;
                member_args.workspace = std::nullopt; // Prevent recursion loop

                if (auto res = action(member_args); !res) {
                    catalyst::logger.error("Test failed for member: {}", name);
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

    catalyst::logger.debug("Composing profiles.");
    auto res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(res.error());
    }
    const utils::yaml::Configuration &profile_comp = res.value();

    catalyst::logger.debug("Running pre-test hooks.");
    if (auto hook_res = hooks::preTest(profile_comp); !hook_res) {
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
        return std::unexpected("profile does not build a binary target");
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
        return std::unexpected("unable to figure out executable name. "
                               "manifest.name and manifest.provides are undefined");
    }

    fs::path exe_path = fs::absolute(fs::path(std::format("{}/{}", build_dir, exe)));
    std::string command = commandStr(exe_path, args.params);
    catalyst::logger.debug("Executing command: {}", command);

    std::vector<std::string> exec_args;
    exec_args.push_back(exe_path.string());
    exec_args.insert(exec_args.end(), args.params.begin(), args.params.end());

    if (int res = catalyst::processExec(std::move(exec_args)).value().get(); res) {
        return std::unexpected(
            std::format("Target executable: {} exited with failure code: {}", exe_path.string(), res));
    }

    catalyst::logger.debug("Running post-test hooks.");
    if (auto res = hooks::postTest(profile_comp); !res) {
        return res;
    }

    catalyst::logger.debug("Test subcommand finished successfully.");
    return {};
}

} // namespace catalyst::test
