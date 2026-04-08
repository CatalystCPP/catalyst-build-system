// a helper function to just get ldlibs for use in run/action.cpp
#include <filesystem>
#include <string>
#include <vector>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/yaml.h"

namespace fs = std::filesystem;
namespace catalyst::generate {

// NOTE: used for run::action. Needs to be updated to use find_*.
std::expected<std::string, std::string> libPath(const YAML::Node &profile,
                                                const std::vector<std::string> &profiles,
                                                const catalyst::toolchain::ToolchainDef &tc) {
    catalyst::logger.debug("Calculating LD_LIBRARY_PATH.");
    fs::path build_dir =
        catalyst::utils::yaml::multiplexedBuildDir(profile["manifest"]["dirs"]["build"].as<std::string>(), profiles);
    std::vector<std::string> lib_dirs{fs::absolute("catalyst-libs").string()};

    if (const char *vcpkg_root = std::getenv("VCPKG_ROOT"); vcpkg_root != nullptr) {
#if defined(_WIN32)
        const char *triplet = "x64-windows";
#elif defined(__APPLE__)
        const char *triplet = "x64-osx";
#else
        const char *triplet = "x64-linux";
#endif
        lib_dirs.push_back((fs::path(vcpkg_root) / "installed" / triplet / "lib").string());
    } else {
        logger.warn("VCPKG_ROOT environment variable is not defined.");
    }

    if (auto deps = profile["dependencies"]; deps && deps.IsSequence()) {
        for (const auto &dep : deps) {
            if (auto res = findDep(build_dir.string(), dep, tc); !res) {
                catalyst::logger.error(
                    "Failed to resolve dependency {}: {}", dep["name"].as<std::string>(), res.error());
            } else {
                lib_dirs.insert(lib_dirs.end(), res->lib_dirs.begin(), res->lib_dirs.end());
            }
        }
    }

    std::string result;
    for (size_t i = 0; i < lib_dirs.size(); ++i) {
        result += lib_dirs[i];
        if (i + 1 < lib_dirs.size()) {
#if defined(_WIN32)
            result += ";";
#else
            result += ":";
#endif
        }
    }
    return result;
}

} // namespace catalyst::generate
