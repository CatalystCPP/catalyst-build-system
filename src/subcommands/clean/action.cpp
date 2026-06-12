#include <algorithm>
#include <format>
#include <string>
#include <tuple>

#include <catalyst/hooks.hpp>
#include <catalyst/subcommands/clean.hpp>

#include "catalyst/dir_guard.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::clean {
namespace fs = std::filesystem;

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.debug("Clean subcommand invoked.");

    if (parse_args.workspace) {
        bool is_root = false;
        try {
            is_root = fs::equivalent(parse_args.workspace->getRoot(), fs::current_path());
        } catch (...) {
            std::ignore;
        }

        if (is_root) {
            catalyst::logger.info("Cleaning all workspace members.");
            bool any_failed = false;

            for (const auto &[name, member] : parse_args.workspace->getMembers()) {
                catalyst::logger.info("Cleaning member: {}", name);
                catalyst::DirectoryChangeGuard dir_guard(member.path);

                Parse member_args = parse_args;
                member_args.workspace = std::nullopt; // Prevent recursion loop

                if (auto res = action(member_args); !res) {
                    catalyst::logger.error("Clean failed for member: {}", name);
                    any_failed = true;
                }
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
    auto res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(res.error());
    }
    const utils::yaml::Configuration &profile_comp = res.value();

    catalyst::logger.debug("Running pre-clean hooks.");
    if (auto hook_res = hooks::preClean(profile_comp); !hook_res) {
        return hook_res;
    }

    auto build_dir =
        catalyst::utils::yaml::multiplexedBuildDir(profile_comp.getString("manifest.dirs.build").value_or(""), profiles)
            .string();
    auto generator_opt = profile_comp.getString("meta.generator");
    if (!generator_opt) {
        return std::unexpected("Unable to get value for meta.generator");
    }
    const std::string &generator = *generator_opt;
    catalyst::logger.debug("Cleaning build directory: {}", build_dir);

    std::vector<std::string> clean_cmd = {generator, "-C", build_dir, "-t", "clean"};
    if (parse_args.intermediates) {
        if (generator == "ninja") {
            clean_cmd.emplace_back("-r");
            clean_cmd.emplace_back("cxx_compile");
            clean_cmd.emplace_back("cc_compile");
        } else if (generator == "cob") {
            clean_cmd.emplace_back("-i");
        } else {
            return std::unexpected(std::format("Generator: {} does not support cleaning intermediates.", generator));
        }
    }

    if (generator == "ninja" || generator == "cob") {
        if (int rtn = catalyst::processExec(std::move(clean_cmd)).value().get(); rtn != 0) {
            std::string cmd_str = clean_cmd[0];
            for (size_t i = 1; i < clean_cmd.size(); ++i) {
                cmd_str += " " + clean_cmd[i];
            }
            return std::unexpected(std::format("Command: {} failed with exit code: {}", cmd_str, rtn));
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
