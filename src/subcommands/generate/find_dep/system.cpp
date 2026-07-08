#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::generate {
namespace {
std::optional<FindRes> findSystemFromPkgConfig(const std::string &dep_name,
                                               const catalyst::toolchain::ToolchainDef &tc) {
    auto cflags_res = processExecStdout({"pkg-config", "--cflags", dep_name});
    auto link_dirs_res = processExecStdout({"pkg-config", "--libs-only-L", dep_name});
    auto link_libs_res = processExecStdout({"pkg-config", "--libs-only-l", "--libs-only-other", dep_name});

    if (!(cflags_res && link_dirs_res && link_libs_res)) {
        return std::nullopt;
    }

    FindRes result{};
    appendPkgConfigFlags(*cflags_res, tc, PkgConfigFlagBucket::Compile, result);
    appendPkgConfigFlags(*link_dirs_res, tc, PkgConfigFlagBucket::LinkDirs, result);
    appendPkgConfigFlags(*link_libs_res, tc, PkgConfigFlagBucket::Link, result);

    catalyst::logger.debug(
        "Resolved via pkg-config: cflags='{}' L='{}' l='{}'", result.inc_path, result.lib_path, result.libs);
    return result;
}
} // namespace

Result<FindRes> findSystem(ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc) {
    namespace yaml = catalyst::utils::yaml;
    auto dep_name = yaml::asString(yaml::child(dep, "name")).value_or("");
    catalyst::logger.debug("Resolving system dependency: {}", dep_name);

    std::string linkage = yaml::asString(yaml::child(dep, "linkage")).value_or("shared"); // assume shared

    std::optional<std::string> explicit_include = yaml::asString(yaml::child(dep, "include"));
    std::optional<std::string> explicit_lib = yaml::asString(yaml::child(dep, "lib"));
    bool has_explicit_include = explicit_include.has_value();
    bool has_explicit_lib = explicit_lib.has_value();

    std::string inc_path;
    std::string lib_path;
    std::string libs;
    std::vector<std::string> lib_dirs;

    if (has_explicit_include) {
        inc_path += " " + catalyst::toolchain::expandTemplate(tc.flags.include_dir, {{"path", *explicit_include}});
    }

    if (has_explicit_lib) {
        lib_path += " " + catalyst::toolchain::expandTemplate(tc.flags.lib_dir, {{"path", *explicit_lib}});
        lib_dirs.push_back(*explicit_lib);
    }

    if (has_explicit_include && has_explicit_lib) {
        // Fully explicit, just add library name
        if (linkage == "static" || linkage == "shared") {
            libs = " " + catalyst::toolchain::expandTemplate(tc.flags.lib, {{"name", dep_name}});
        }
        return FindRes{.lib_path = lib_path, .inc_path = inc_path, .libs = libs, .lib_dirs = lib_dirs};
    }

    // Try pkg-config if not fully explicit

    if (auto res = findSystemFromPkgConfig(dep_name, tc); res) {
        if (!has_explicit_include) {
            inc_path += " " + res->inc_path;
        }
        if (!has_explicit_lib) {
            lib_path += " " + res->lib_path;
            lib_dirs.insert(lib_dirs.end(), res->lib_dirs.begin(), res->lib_dirs.end());
        }
        libs += " " + res->libs;
        return FindRes{.lib_path = lib_path, .inc_path = inc_path, .libs = libs, .lib_dirs = lib_dirs};
    }

    catalyst::logger.debug("pkg-config failed for {}, falling back to default paths.", dep_name);

    // Fallback defaults
    if (!has_explicit_include) {
#if defined(_WIN32)
#elif defined(__APPLE__)
        inc_path += " " + catalyst::toolchain::expand_template(tc.flags.include_dir, {{"path", "/usr/local/include"}});
#else
        inc_path += " " + catalyst::toolchain::expandTemplate(tc.flags.include_dir, {{"path", "/usr/include"}});
#endif
    }

    if (!has_explicit_lib) {
#if defined(_WIN32)
#elif defined(__APPLE__)
        lib_path += " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", "/usr/local/lib"}});
        lib_dirs.push_back("/usr/local/lib");
#else
        lib_path += " " + catalyst::toolchain::expandTemplate(tc.flags.lib_dir, {{"path", "/usr/lib"}});
        lib_dirs.emplace_back("/usr/lib");
#endif
    }

    if (linkage == "static" || linkage == "shared") {
        libs += " " + catalyst::toolchain::expandTemplate(tc.flags.lib, {{"name", dep_name}});
    }
    return FindRes{.lib_path = lib_path, .inc_path = inc_path, .libs = libs, .lib_dirs = lib_dirs};
}
} // namespace catalyst::generate
