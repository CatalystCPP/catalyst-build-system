#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

#ifdef _WIN32
constexpr const char *const TRIPLET = "x64-windows";
constexpr char DELIMITER = ';';
#elif defined(__APPLE__)
constexpr const char *const TRIPLET = "x64-osx";
constexpr char DELIMITER = ':';
#else
constexpr const char *const TRIPLET = "x64-linux";
constexpr char DELIMITER = ':';
#endif

auto catalyst::generate::libPath(const utils::yaml::Configuration &profile_comp,
                                 const std::vector<std::string> &profiles,
                                 const catalyst::toolchain::ToolchainDef &tc) -> Result<std::string> {
    namespace yaml = catalyst::utils::yaml;
    catalyst::logger.debug("Calculating LD_LIBRARY_PATH.");
    std::filesystem::path build_dir = catalyst::utils::yaml::multiplexedBuildDir(
        profile_comp.getString("manifest.dirs.build").value_or(""), profiles);
    std::vector<std::string> lib_dirs{std::filesystem::absolute("catalyst-libs").string()};

    if (const char *vcpkg_root = std::getenv("VCPKG_ROOT"); vcpkg_root)
        lib_dirs.push_back((std::filesystem::path(vcpkg_root) / "installed" / TRIPLET / "lib").string());
    else
        logger.warn("VCPKG_ROOT environment variable is not defined.");

    if (auto deps = yaml::child(profile_comp.rootRef(), "dependencies"); deps.readable() && deps.is_seq())
        for (ryml::ConstNodeRef dep : deps.children()) {
            if (auto res = findDep(build_dir.string(), dep, tc); !res)
                catalyst::logger.error("Failed to resolve dependency {}: {}",
                                       yaml::asString(yaml::child(dep, "name")).value_or("<unnamed>"),
                                       res.error());
            else
                lib_dirs.insert(lib_dirs.end(), res->lib_dirs.begin(), res->lib_dirs.end());
        }

    return std::string{std::from_range, lib_dirs | std::views::join_with(DELIMITER)};
}
