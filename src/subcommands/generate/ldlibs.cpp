#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

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

auto catalyst::generate::libPath(const YAML::Node &profile,
                                 const std::vector<std::string> &profiles,
                                 const catalyst::toolchain::ToolchainDef &tc)
    -> std::expected<std::string, std::string> {
    catalyst::logger.debug("Calculating LD_LIBRARY_PATH.");
    std::filesystem::path build_dir =
        catalyst::utils::yaml::multiplexedBuildDir(profile["manifest"]["dirs"]["build"].as<std::string>(), profiles);
    std::vector<std::string> lib_dirs{std::filesystem::absolute("catalyst-libs").string()};

    if (const char *vcpkg_root = std::getenv("VCPKG_ROOT"); vcpkg_root)
        lib_dirs.push_back((std::filesystem::path(vcpkg_root) / "installed" / TRIPLET / "lib").string());
    else
        logger.warn("VCPKG_ROOT environment variable is not defined.");

    if (auto deps = profile["dependencies"]; deps && deps.IsSequence())
        for (const auto &dep : deps) {
            if (auto res = findDep(build_dir.string(), dep, tc); !res)
                catalyst::logger.error(
                    "Failed to resolve dependency {}: {}", dep["name"].as<std::string>(), res.error());
            else
                lib_dirs.insert(lib_dirs.end(), res->lib_dirs.begin(), res->lib_dirs.end());
        }

    return std::string{std::from_range, lib_dirs | std::views::join_with(DELIMITER)};
}
