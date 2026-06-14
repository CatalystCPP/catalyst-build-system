#include <expected>
#include <filesystem>
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

Result<FindRes>
findGit(const std::string &build_dir, ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc) {
    namespace yaml = catalyst::utils::yaml;
    std::string dep_name = yaml::asString(yaml::child(dep, "name")).value_or("<unnamed>");
    catalyst::logger.debug("Resolving git dependency: {}", dep_name);

    fs::path dep_path = fs::path(build_dir) / "catalyst-libs" / dep_name;

    DirectoryChangeGuard dg(dep_path);

    std::vector<std::string> profiles =
        yaml::asStringVector(yaml::child(dep, "profiles")).value_or(std::vector<std::string>{});

    catalyst::logger.debug("Composing profiles for git dependency.");
    auto pc = catalyst::generate::profileComposition(profiles);

    if (!pc) {
        return std::unexpected(pc.error());
    }

    const utils::yaml::Configuration &profile = *pc;

    std::string include_path_flags;
    if (auto dep_includes = profile.getStringVector("manifest.dirs.include")) {
        for (const auto &include_dir : *dep_includes) {
            fs::path abs_include_path = fs::absolute(dep_path / include_dir);
            catalyst::logger.debug("Adding include path: {}", abs_include_path.string());
            include_path_flags +=
                " " + catalyst::toolchain::expand_template(tc.flags.include_dir, {{"path", abs_include_path.string()}});
        }
    }

    std::string library_path_flags;
    std::vector<std::string> lib_dirs;
    if (auto dep_build_dir_str = profile.getString("manifest.dirs.build")) {
        fs::path dep_build_dir = catalyst::utils::yaml::multiplexedBuildDir(*dep_build_dir_str, profiles);
        fs::path lib_path = fs::absolute(dep_path / dep_build_dir);
        catalyst::logger.debug("Adding library path: {}", lib_path.string());
        library_path_flags +=
            " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", lib_path.string()}});
        lib_dirs.push_back(lib_path.string());
    }

    std::string libs_flags;
    if (auto lib_name = profile.getString("manifest.name")) {
        catalyst::logger.debug("Adding library: {}", *lib_name);
        libs_flags += " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", *lib_name}});
    }

    return FindRes{
        .lib_path = library_path_flags, .inc_path = include_path_flags, .libs = libs_flags, .lib_dirs = lib_dirs};
}
} // namespace catalyst::generate
