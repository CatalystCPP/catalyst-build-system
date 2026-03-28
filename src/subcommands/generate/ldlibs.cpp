// a helper function to just get ldlibs for use in run/action.cpp
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/yaml.h"

namespace fs = std::filesystem;
namespace catalyst::generate {

namespace {
std::string ld_filter(std::string &ldflags) {
    std::stringstream ss(ldflags);
    std::string item;
    std::vector<std::string> tokens;
    while (std::getline(ss, item, ' ')) {
        if (item.rfind("-L", 0) == 0) {
            tokens.push_back(fs::absolute(item.substr(2)).string());
        }
    }

    std::string result;
    for (size_t i = 0; i < tokens.size(); ++i) {
        result += tokens[i];
        if (i < tokens.size() - 1) {
#if defined(_WIN32)
            result += ";";
#else
            result += ":";
#endif
        }
    }
    return result;
}
} // namespace

// NOTE: used for run::action. Needs to be updated to use find_*.
std::expected<std::string, std::string> libPath(const YAML::Node &profile,
                                                 const std::vector<std::string> &profiles) {
    catalyst::logger.debug("Calculating LD_LIBRARY_PATH.");
    fs::path current_dir = fs::current_path();
    fs::path build_dir = catalyst::utils::yaml::multiplexedBuildDir(
        profile["manifest"]["dirs"]["build"].as<std::string>(), profiles);
    std::string ldflags = "-Lcatalyst-libs";

    if (const char *vcpkg_root = std::getenv("VCPKG_ROOT"); vcpkg_root != nullptr) {
#if defined(_WIN32)
        const char *triplet = "x64-windows";
#elif defined(__APPLE__)
        const char *triplet = "x64-osx";
#else
        const char *triplet = "x64-linux";
#endif
        ldflags += std::format(" -L{}", (fs::path(vcpkg_root) / "installed" / triplet / "lib").string());
    } else {
        logger.warn("VCPKG_ROOT environment variable is not defined.");
    }

    if (auto deps = profile["dependencies"]; deps && deps.IsSequence()) {
        for (const auto &dep : deps) {
            if (auto res = findDep(build_dir.string(), dep); !res) {
                catalyst::logger.error("Failed to resolve dependency {}: {}", dep["name"].as<std::string>(), res.error());
            } else {
                ldflags += " " + res.value().lib_path;
            }
        }
    }
    return ld_filter(ldflags);
}

} // namespace catalyst::generate
