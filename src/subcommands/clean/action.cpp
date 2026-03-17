#include <algorithm>
#include <format>
#include <string>
#include <tuple>

#include <catalyst/hooks.hpp>
#include <catalyst/subcommands/clean.hpp>
#include <yaml-cpp/node/node.h>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::clean {
namespace fs = std::filesystem;

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.debug("Clean subcommand invoked.");

    if (parse_args.workspace) {
        fs::path current = fs::current_path();
        bool is_root = false;
        try {
            is_root = fs::equivalent(parse_args.workspace->getRoot(), current);
        } catch (...) {
            std::ignore;
        }

        if (is_root) {
            catalyst::logger.info("Cleaning all workspace members.");
            bool any_failed = false;

            for (const auto &[name, member] : parse_args.workspace->getMembers()) {
                catalyst::logger.info("Cleaning member: {}", name);
                fs::current_path(member.path);

                Parse member_args = parse_args;
                member_args.workspace = std::nullopt; // Prevent recursion loop

                if (auto res = action(member_args); !res) {
                    catalyst::logger.error("Clean failed for member: {}", name);
                    any_failed = true;
                }
                fs::current_path(current);
            }
            if (any_failed)
                return std::unexpected("Clean failed for some members.");
            return {};
        }
    }

    auto profiles = parse_args.profiles;
    if (std::ranges::find(profiles, "common") == profiles.end()) {
        profiles.insert(profiles.begin(), "common");
    }

    catalyst::logger.debug("Composing profiles.");
    YAML::Node profile_comp;
    auto res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(res.error());
    }
    profile_comp = res.value();

    catalyst::logger.debug("Running pre-clean hooks.");
    if (auto res = hooks::preClean(profile_comp); !res) {
        return res;
    }

    auto build_dir = profile_comp["manifest"]["dirs"]["build"].as<std::string>();
    auto generator = profile_comp["meta"]["generator"].as<std::string>();
    catalyst::logger.debug("Cleaning build directory: {}", build_dir);
    if (generator == "ninja") {
        if (int rtn = catalyst::processExec({"ninja", "-C", build_dir, "-t", "clean"}).value().get(); rtn != 0) {
            return std::unexpected(
                std::format("Command: ninja -C {} -t clean failed with exit code: {}", build_dir, rtn));
        }
    } else if (generator == "cbe") {
        if (int rtn = catalyst::processExec({"cbe", "-C", build_dir, "-t", "clean"}).value().get(); rtn != 0) {
            return std::unexpected(
                std::format("Command: cbe -C {} -t clean failed with exit code: {}", build_dir, rtn));
        }
    }

    catalyst::logger.debug("Running post-clean hooks.");
    if (auto res = hooks::postClean(profile_comp); !res) {
        return res;
    }

    catalyst::logger.debug("Clean subcommand finished successfully.");
    return {};
}
} // namespace catalyst::clean
