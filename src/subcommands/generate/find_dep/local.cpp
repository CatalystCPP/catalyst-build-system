#include <expected>
#include <format>
#include <string>

#include "catalyst/dir_guard.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/node/node.h"

namespace catalyst::generate {
namespace fs = std::filesystem;

std::expected<FindRes, std::string> findLocal(const YAML::Node &dep, const catalyst::toolchain::ToolchainDef &tc) {
    catalyst::logger.debug("Resolving local dependency: {}", dep["name"].as<std::string>());

    if (!dep["path"]) {
        return std::unexpected(
            std::format("Local Dependency: {} does not define path.", dep["name"].as<std::string>()));
    }

    fs::path dep_path = dep["path"].as<std::string>();
    catalyst::logger.debug("Changing directory to: {}", dep_path.string());
    catalyst::DirectoryChangeGuard dg(dep_path);

    std::vector<std::string> profiles{};
    std::vector<std::string> features{};
    if (dep["profiles"] && dep["profiles"].IsSequence())
        profiles = dep["profiles"].as<std::vector<std::string>>();

    if (profiles.empty())
        profiles.emplace_back("common");

    if (dep["using"] && dep["using"].IsSequence())
        features = dep["using"].as<std::vector<std::string>>();
    catalyst::logger.debug("Composing profiles for local dependency.");
    auto pc = catalyst::generate::profileComposition(profiles);

    if (!pc) {
        return std::unexpected(pc.error());
    }
    YAML::Node profile = pc.value();

    // DO NOT rebuild here. This should have been done in fetch::fetch_local or something.

    // Add include directories
    std::string include_path;
    if (auto includes = profile["manifest"]["dirs"]["include"]; includes && includes.IsSequence()) {
        for (const auto &dir : includes.as<std::vector<std::string>>()) {
            auto curr = fs::absolute(dep_path / dir);
            catalyst::logger.debug("Adding include path: {}", curr.string());
            include_path += " " + catalyst::toolchain::expand_template(tc.flags.include_dir, {{"path", curr.string()}});
        }
    }

    // Add library directory
    std::string library_path;
    if (auto build_dir_node = profile["manifest"]["dirs"]["build"]) {
        fs::path build_dir = catalyst::utils::yaml::multiplexedBuildDir(build_dir_node.as<std::string>(), profiles);
        auto lib_path = fs::absolute(dep_path / build_dir);
        catalyst::logger.debug("Adding library path: {}", lib_path.string());
        library_path += " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", lib_path.string()}});
    }

    // Add library
    std::string libs;
    if (auto dep_name_node = profile["manifest"]["name"]) {
        auto dep_name = dep_name_node.as<std::string>();
        catalyst::logger.debug("Adding library: {}", dep_name);
        libs += " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", dep_name}});
    }

    return FindRes{.lib_path = library_path, .inc_path = include_path, .libs = libs};
}
} // namespace catalyst::generate
