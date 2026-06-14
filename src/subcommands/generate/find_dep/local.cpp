#include <expected>
#include <format>
#include <string>

#include "catalyst/utils/result.hpp"
#include "catalyst/dir_guard.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::generate {
namespace fs = std::filesystem;

Result<FindRes> findLocal(ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc) {
    namespace yaml = catalyst::utils::yaml;
    std::string dep_name = yaml::asString(yaml::child(dep, "name")).value_or("<unnamed>");
    catalyst::logger.debug("Resolving local dependency: {}", dep_name);

    auto dep_path_opt = yaml::asString(yaml::child(dep, "path"));
    if (!dep_path_opt) {
        return std::unexpected(std::format("Local Dependency: {} does not define path.", dep_name));
    }

    fs::path dep_path = *dep_path_opt;
    catalyst::logger.debug("Changing directory to: {}", dep_path.string());
    catalyst::DirectoryChangeGuard dg(dep_path);

    std::vector<std::string> profiles =
        yaml::asStringVector(yaml::child(dep, "profiles")).value_or(std::vector<std::string>{});
    if (profiles.empty())
        profiles.emplace_back("common");

    catalyst::logger.debug("Composing profiles for local dependency.");
    auto pc = catalyst::generate::profileComposition(profiles);

    if (!pc) {
        return std::unexpected(pc.error());
    }
    const utils::yaml::Configuration &profile = pc.value();

    // DO NOT rebuild here. This should have been done in fetch::fetch_local or something.

    // Add include directories
    std::string include_path;
    if (auto includes = profile.getStringVector("manifest.dirs.include")) {
        for (const auto &dir : *includes) {
            auto curr = fs::absolute(dep_path / dir);
            catalyst::logger.debug("Adding include path: {}", curr.string());
            include_path += " " + catalyst::toolchain::expand_template(tc.flags.include_dir, {{"path", curr.string()}});
        }
    }

    // Add library directory
    std::string library_path;
    std::vector<std::string> lib_dirs;
    if (auto build_dir_str = profile.getString("manifest.dirs.build")) {
        fs::path build_dir = catalyst::utils::yaml::multiplexedBuildDir(*build_dir_str, profiles);
        auto lib_path = fs::absolute(dep_path / build_dir);
        catalyst::logger.debug("Adding library path: {}", lib_path.string());
        library_path += " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", lib_path.string()}});
        lib_dirs.push_back(lib_path.string());
    }

    // Add library
    std::string libs;
    if (auto lib_name = profile.getString("manifest.name")) {
        catalyst::logger.debug("Adding library: {}", *lib_name);
        libs += " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", *lib_name}});
    }

    return FindRes{.lib_path = library_path, .inc_path = include_path, .libs = libs, .lib_dirs = lib_dirs};
}
} // namespace catalyst::generate
