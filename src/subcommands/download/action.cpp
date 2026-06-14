#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <random>

#include "catalyst/dir_guard.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/build.hpp"
#include "catalyst/subcommands/download.hpp"
#include "catalyst/subcommands/install.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"

namespace fs = std::filesystem;

namespace catalyst::download {

namespace {
std::string randomString(size_t length) {
    std::random_device rd;
    std::mt19937 rng(rd());

    constexpr std::string_view CHARSET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const size_t max_index = CHARSET.size() - 1;
    std::uniform_int_distribution<size_t> dist(0, max_index);

    std::string str(length, '\0');
    for (size_t i = 0; i < length; ++i) {
        str[i] = CHARSET[dist(rng)];
    }
    return str;
}
} // namespace

Result<void> action(const Parse &args) {
    catalyst::logger.debug("Download subcommand invoked.");

    constexpr size_t TEMP_DIR_NAME_LEN = 8UZ;
    auto absolute_target_path = fs::absolute(args.target_path);
    auto temp_dir = fs::temp_directory_path() / std::format("catalyst_dl_{}", randomString(TEMP_DIR_NAME_LEN));

    catalyst::logger.info("Cloning {} into temporary directory {}", args.git_remote, temp_dir.string());

    std::vector<std::string> clone_cmd = {"git", "clone"};
    if (!args.git_branch.empty()) {
        clone_cmd.insert(clone_cmd.end(), {"--branch", args.git_branch});
    }
    clone_cmd.push_back(args.git_remote);
    clone_cmd.push_back(temp_dir.string());

    auto res = catalyst::processExec(std::move(clone_cmd));
    if (!res)
        return std::unexpected(res.error());
    if (auto exit_code = res.value().get(); exit_code != 0)
        return std::unexpected(std::format("Git clone failed with exit code {}", exit_code));
    if (!fs::exists(temp_dir))
        return std::unexpected("Failed to create temporary directory for clone.");

    catalyst::DirectoryChangeGuard scoped_dir(temp_dir);

    catalyst::logger.info("Building downloaded project with profiles: {} and features: {}",
                          std::format("{}", args.profiles),
                          std::format("{}", args.enabled_features));
    catalyst::build::Parse build_args{
        .regen = false,
        .force_rebuild = false,
        .force_refetch = false,
        .workspace_build = false,
        .watch = false,
        .package = "",
        .profiles = args.profiles,
        .enabled_features = args.enabled_features,
        .backend = args.backend,
        .workspace = std::nullopt,
    };

    if (auto res = catalyst::build::action(build_args); !res) {
        return std::unexpected(std::format("Build failed: {}", res.error()));
    }

    catalyst::logger.info("Installing project to {}...", absolute_target_path.string());
    if (auto res = catalyst::install::action({
            .source_path = temp_dir,
            .target_path = absolute_target_path,
            .profiles = args.profiles,
        });
        !res) {
        return res;
    }

    catalyst::logger.info("Successfully downloaded and installed {}.", args.git_remote);
    std::filesystem::remove_all(temp_dir);
    return {};
}
} // namespace catalyst::download
