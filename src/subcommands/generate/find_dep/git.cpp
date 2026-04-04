#include <expected>
#include <filesystem>
#include <format>
#include <string>

#include "catalyst/dir_guard.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/node/node.h"

namespace catalyst::generate {
namespace fs = std::filesystem;

std::expected<FindRes, std::string> findGit(const std::string &build_dir, const YAML::Node &dep) {
    auto dep_name = dep["name"].as<std::string>();
    catalyst::logger.debug("Resolving git dependency: {}", dep_name);

    fs::path dep_path = fs::path(build_dir) / "catalyst-libs" / dep_name;

    DirectoryChangeGuard dg(dep_path);

    std::vector<std::string> profiles{};
    if (dep["profiles"] && dep["profiles"].IsSequence())
        profiles = dep["profiles"].as<std::vector<std::string>>();

    catalyst::logger.debug("Composing profiles for git dependency.");
    std::expected<YAML::Node, std::string> pc = catalyst::generate::profileComposition(profiles);

    if (!pc) {
        return std::unexpected(pc.error());
    }

    YAML::Node profile = *pc;

    std::string include_path_flags;
    if (auto dep_includes_node = profile["manifest"]["dirs"]["include"];
        dep_includes_node && dep_includes_node.IsSequence()) {
        for (const auto &include_dir : dep_includes_node.as<std::vector<std::string>>()) {
            fs::path abs_include_path = fs::absolute(dep_path / include_dir);
            catalyst::logger.debug("Adding include path: {}", abs_include_path.string());
            include_path_flags += std::format(" -I{}", abs_include_path.string());
        }
    }

    std::string library_path_flags;
    if (auto dep_build_dir_node = profile["manifest"]["dirs"]["build"]) {
        fs::path dep_build_dir =
            catalyst::utils::yaml::multiplexedBuildDir(dep_build_dir_node.as<std::string>(), profiles);
        fs::path lib_path = fs::absolute(dep_path / dep_build_dir);
        catalyst::logger.debug("Adding library path: {}", lib_path.string());
        library_path_flags += std::format(" -L{}", lib_path.string());
    }

    std::string libs_flags;
    if (auto dep_name_node = profile["manifest"]["name"]) {
        auto lib_name = dep_name_node.as<std::string>();
        catalyst::logger.debug("Adding library: {}", lib_name);
        libs_flags += std::format(" -l{}", lib_name);
    }

    return FindRes{.lib_path = library_path_flags, .inc_path = include_path_flags, .libs = libs_flags};
}
} // namespace catalyst::generate
