#include <expected>
#include <optional>
#include <string>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::generate {
std::optional<FindRes> findSystemFromPkgConfig(const std::string &dep_name);

std::expected<FindRes, std::string> findSystem(const YAML::Node &dep, const catalyst::toolchain::ToolchainDef &tc) {
    auto dep_name = dep["name"].as<std::string>();
    catalyst::logger.debug("Resolving system dependency: {}", dep_name);

    std::string linkage; // assume shared
    if (dep["linkage"] && dep["linkage"].IsScalar()) {
        linkage = dep["linkage"].as<std::string>();
    } else {
        linkage = "shared";
    }

    bool has_explicit_include = dep["include"] && dep["include"].IsScalar();
    bool has_explicit_lib = dep["lib"] && dep["lib"].IsScalar();

    std::string inc_path;
    std::string lib_path;
    std::string libs;

    if (has_explicit_include) {
        inc_path += " " + catalyst::toolchain::expand_template(tc.flags.include_dir,
                                                               {{"path", dep["include"].as<std::string>()}});
    }

    if (has_explicit_lib) {
        lib_path +=
            " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", dep["lib"].as<std::string>()}});
    }

    if (has_explicit_include && has_explicit_lib) {
        // Fully explicit, just add library name
        if (linkage == "static" || linkage == "shared") {
            libs = " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", dep_name}});
        }
        return FindRes{.lib_path = lib_path, .inc_path = inc_path, .libs = libs};
    }

    // Try pkg-config if not fully explicit

    if (auto res = findSystemFromPkgConfig(dep_name); res) {
        std::string cflags_val = res->lib_path;
        std::string lib_path_val = res->lib_path;
        std::string libs_val = res->libs;
        auto trim = [](std::string &s) {
            if (auto last = s.find_last_not_of(" \t\n"); last != std::string::npos)
                s.erase(last + 1);
        };

        trim(cflags_val);
        trim(lib_path_val);
        trim(libs_val);

        catalyst::logger.debug(
            "Resolved via pkg-config: cflags='{}' L='{}' l='{}'", cflags_val, lib_path_val, libs_val);

        if (!has_explicit_include)
            inc_path += " " + cflags_val;
        if (!has_explicit_lib)
            lib_path += " " + lib_path_val;

        libs += " " + libs_val;

        return FindRes{.lib_path = lib_path, .inc_path = inc_path, .libs = libs};
    }

    catalyst::logger.debug("pkg-config failed for {}, falling back to default paths.", dep_name);

    // Fallback defaults
    if (!has_explicit_include) {
#if defined(_WIN32)
#elif defined(__APPLE__)
        inc_path += " " + catalyst::toolchain::expand_template(tc.flags.include_dir, {{"path", "/usr/local/include"}});
#else
        inc_path += " " + catalyst::toolchain::expand_template(tc.flags.include_dir, {{"path", "/usr/include"}});
#endif
    }

    if (!has_explicit_lib) {
#if defined(_WIN32)
#elif defined(__APPLE__)
        lib_path += " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", "/usr/local/lib"}});
#else
        lib_path += " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", "/usr/lib"}});
#endif
    }

    if (linkage == "static" || linkage == "shared") {
        libs += " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", dep_name}});
    }
    return FindRes{.lib_path = lib_path, .inc_path = inc_path, .libs = libs};
}

std::optional<FindRes> findSystemFromPkgConfig(const std::string &dep_name) {
    auto res_cflags = processExecStdout({"pkg-config", "--cflags", dep_name});
    auto res_L = processExecStdout({"pkg-config", "--libs-only-L", dep_name});
    auto res_l = processExecStdout({"pkg-config", "--libs-only-l", "--libs-only-other", dep_name});

    if (res_cflags && res_L && res_l) {
        std::string cflags_val = *res_cflags;
        std::string L_val = *res_L;
        std::string l_val = *res_l;

        auto trim = [](std::string &s) {
            if (auto last = s.find_last_not_of(" \t\n"); last != std::string::npos)
                s.erase(last + 1);
        };

        trim(cflags_val);
        trim(L_val);
        trim(l_val);

        catalyst::logger.debug("Resolved via pkg-config: cflags='{}' L='{}' l='{}'", cflags_val, L_val, l_val);

        return FindRes{.lib_path = L_val, .inc_path = cflags_val, .libs = l_val};
    }
    return std::nullopt;
}
} // namespace catalyst::generate
