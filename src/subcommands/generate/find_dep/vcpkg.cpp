#include <unistd.h>

#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <unordered_map>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::generate {
std::expected<FindRes, std::string> findVcpkg(const YAML::Node &dep, const catalyst::toolchain::ToolchainDef &tc) {
    auto triplet = dep["triplet"].as<std::string>();
    std::string linkage;
    if (dep["linkage"] && dep["linkage"].IsScalar()) {
        linkage = dep["linkage"].Scalar();
    } else {
        linkage = "shared";
    }

    const char *vcpkg_root_env = std::getenv("VCPKG_ROOT");
    if (vcpkg_root_env == nullptr) {
        return std::unexpected(
            std::format("VCPKG_ROOT is not set, cannot resolve vcpkg dependency '{}'.", dep["name"].as<std::string>()));
    }

    std::string dep_name = dep["name"].as<std::string>();
    catalyst::logger.debug("Resolving vcpkg dependency: {}", dep_name);

    // Construct the path to the library directory within the specific package folder
    // $VCPKG_ROOT/packages/<package>_<triplet>/lib
    namespace fs = std::filesystem;

    fs::path vcpkg_root(vcpkg_root_env);
    fs::path package_dir_name = std::format("{}_{}", dep_name, triplet);
    fs::path lib_path = vcpkg_root / "packages" / package_dir_name / "lib";

    fs::path pkg_config_dir = lib_path / "pkgconfig";
    fs::path pc_file = pkg_config_dir / std::format("{}.pc", dep_name);

    if (fs::exists(pc_file)) {
        catalyst::logger.debug("Found pkg-config file for {}: {}", dep_name, pc_file.string());

        std::unordered_map<std::string, std::string> env;
        if (const char *path_env = std::getenv("PATH")) {
            env["PATH"] = path_env;
        }
        env["PKG_CONFIG_PATH"] = pkg_config_dir.string();

        auto res_L = processExecStdout({"pkg-config", "--libs-only-L", dep_name}, std::nullopt, env);
        auto res_l =
            processExecStdout({"pkg-config", "--libs-only-l", "--libs-only-other", dep_name}, std::nullopt, env);

        if (res_L && res_l) {
            std::string L_val = *res_L;
            std::string l_val = *res_l;

            if (auto last = L_val.find_last_not_of(" \t\n"); last != std::string::npos)
                L_val.erase(last + 1);
            if (auto last = l_val.find_last_not_of(" \t\n"); last != std::string::npos)
                l_val.erase(last + 1);

            catalyst::logger.debug("Resolved via pkg-config: L='{}' l='{}'", L_val, l_val);
            return FindRes{.lib_path = L_val,
                           .inc_path = "", // already set in write_variables
                           .libs = l_val};
        } else {
            catalyst::logger.warn("pkg-config failed for {}, falling back.", dep_name);
        }
    }
    catalyst::logger.debug("Did not find pkg-config file for {}: {}", dep_name, pc_file.string());

    std::string library_path, libs;

    if (linkage == "static" || linkage == "shared") {
        if (!fs::exists(lib_path) || !fs::is_directory(lib_path)) {
            catalyst::logger.warn(
                "Could not find library directory for vcpkg package '{}' at: {}", dep_name, lib_path.string());
            libs += " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", dep_name}});
        }
    }

    library_path += " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", lib_path.string()}});
    catalyst::logger.debug("Adding library path: {}", lib_path.string());

    if (linkage == "static" || linkage == "shared") {
// Define the library file extensions based on the operating system.
#if defined(_WIN32)
        const std::vector<std::string> extensions = {".lib"};
#elif defined(__APPLE__)
        const std::vector<std::string> extensions = {".a", ".dylib"};
#else // Linux and other Unix-like systems
        const std::vector<std::string> extensions = {".a", ".so"};
#endif

        // Iterate through the directory and find matching library files.
        for (const auto &entry : fs::directory_iterator(lib_path)) {
            if (entry.is_regular_file()) {
                const fs::path &file_path = entry.path();
                std::string file_ext = file_path.extension().string();

                // Check if the file has one of the target extensions
                for (const auto &expected_ext : extensions) {
                    if (file_ext == expected_ext) {
                        // Convert file path to a linker flag (e.g., "libfmt.a" -> "-lfmt")
                        std::string stem = file_path.stem().string();
                        if (stem.rfind("lib", 0) == 0) { // Check if it starts with "lib"
                            stem = stem.substr(3);
                        }
                        libs += " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", stem}});
                        catalyst::logger.debug("Found and added library: {}", stem);
                        break; // Found a matching extension, move to the next file
                    }
                }
            }
        }
    }

    return FindRes{.lib_path = library_path,
                   .inc_path = "", // already set in write_variables
                   .libs = libs};
}
} // namespace catalyst::generate
