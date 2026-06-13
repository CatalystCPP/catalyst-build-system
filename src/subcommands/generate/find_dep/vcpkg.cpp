#include <unistd.h>

#include <expected>
#include <filesystem>
#include <format>
#include <sstream>
#include <string>
#include <unordered_map>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::generate {
namespace fs = std::filesystem;

namespace {
void append_pkg_config_libs(FindRes &result,
                            const std::string &lib_dirs_output,
                            const std::string &libs_output,
                            const catalyst::toolchain::ToolchainDef &tc) {
    std::stringstream lib_dir_stream(lib_dirs_output);
    std::stringstream libs_stream(libs_output);
    std::string token;

    while (lib_dir_stream >> token) {
        if (token.rfind("-L", 0) == 0) {
            std::string path = token.substr(2);
            result.lib_path += " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", path}});
            result.lib_dirs.push_back(path);
        } else {
            result.lib_path += " " + token;
        }
    }

    while (libs_stream >> token) {
        if (token.rfind("-l", 0) == 0) {
            result.libs += " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", token.substr(2)}});
        } else {
            result.libs += " " + token;
        }
    }
}
} // namespace

std::expected<FindRes, std::string> findVcpkg(ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc) {
    namespace yaml = catalyst::utils::yaml;
    std::string dep_name = yaml::asString(yaml::child(dep, "name")).value_or("<unnamed>");
    auto triplet_opt = yaml::asString(yaml::child(dep, "triplet"));
    if (!triplet_opt) {
        return std::unexpected(std::format("vcpkg dependency '{}' does not define a triplet.", dep_name));
    }
    std::string triplet = *triplet_opt;
    std::string linkage = yaml::asString(yaml::child(dep, "linkage")).value_or("shared");

    const char *vcpkg_root_env = std::getenv("VCPKG_ROOT");
    if (vcpkg_root_env == nullptr) {
        return std::unexpected(std::format("VCPKG_ROOT is not set, cannot resolve vcpkg dependency '{}'.", dep_name));
    }

    catalyst::logger.debug("Resolving vcpkg dependency: {}", dep_name);

    // Construct the path to the library directory within the specific package folder
    // $VCPKG_ROOT/packages/<package>_<triplet>/lib
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
            FindRes result{.lib_path = "", .inc_path = "", .libs = "", .lib_dirs = {}};
            append_pkg_config_libs(result, L_val, l_val, tc);
            return result;
        } else {
            catalyst::logger.warn("pkg-config failed for {}, falling back.", dep_name);
        }
    }
    catalyst::logger.debug("Did not find pkg-config file for {}: {}", dep_name, pc_file.string());

    std::string library_path, libs;
    std::vector<std::string> lib_dirs;

    if (linkage == "static" || linkage == "shared") {
        if (!fs::exists(lib_path) || !fs::is_directory(lib_path)) {
            catalyst::logger.warn(
                "Could not find library directory for vcpkg package '{}' at: {}", dep_name, lib_path.string());
            libs += " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", dep_name}});
        }
    }

    library_path += " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", lib_path.string()}});
    catalyst::logger.debug("Adding library path: {}", lib_path.string());
    lib_dirs.push_back(lib_path.string());

    if (linkage == "static" || linkage == "shared") {
        std::vector<std::string> extensions;
        if (!tc.extensions.static_lib.empty()) {
            extensions.push_back(tc.extensions.static_lib);
        }
        if (!tc.extensions.shared_lib.empty() && tc.extensions.shared_lib != tc.extensions.static_lib) {
            extensions.push_back(tc.extensions.shared_lib);
        }
        if (extensions.empty()) {
#if defined(_WIN32)
            extensions.push_back(".lib");
#elif defined(__APPLE__)
            extensions = {".a", ".dylib"};
#else
            extensions = {".a", ".so"};
#endif
        }

        // Iterate through the directory and find matching library files.
        for (const auto &entry : fs::directory_iterator(lib_path)) {
            if (entry.is_regular_file()) {
                const fs::path &file_path = entry.path();
                std::string file_ext = file_path.extension().string();

                // Check if the file has one of the target extensions
                for (const auto &expected_ext : extensions) {
                    if (file_ext == expected_ext) {
                        // Convert the file name into the toolchain's configured library token.
                        std::string stem = file_path.stem().string();
                        if (!tc.extensions.static_lib_prefix.empty() &&
                            stem.rfind(tc.extensions.static_lib_prefix, 0) == 0) {
                            stem = stem.substr(tc.extensions.static_lib_prefix.size());
                        } else if (!tc.extensions.shared_lib_prefix.empty() &&
                                   stem.rfind(tc.extensions.shared_lib_prefix, 0) == 0) {
                            stem = stem.substr(tc.extensions.shared_lib_prefix.size());
                        } else if (stem.rfind("lib", 0) == 0) {
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
                   .libs = libs,
                   .lib_dirs = lib_dirs};
}
} // namespace catalyst::generate
