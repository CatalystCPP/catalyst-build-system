#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::generate {
namespace fs = std::filesystem;

namespace {
void appendPkgConfigLibs(FindRes &result,
                         const std::string &lib_dirs_output,
                         const std::string &libs_output,
                         const catalyst::toolchain::ToolchainDef &tc) {
    std::stringstream lib_dir_stream(lib_dirs_output);
    std::stringstream libs_stream(libs_output);
    std::string token;

    while (lib_dir_stream >> token) {
        if (token.starts_with("-L")) {
            std::string path = token.substr(2);
            result.lib_path += " " + catalyst::toolchain::expandTemplate(tc.flags.lib_dir, {{"path", path}});
            result.lib_dirs.push_back(path);
        } else {
            result.lib_path += " " + token;
        }
    }

    while (libs_stream >> token) {
        if (token.starts_with("-l")) {
            result.libs += " " + catalyst::toolchain::expandTemplate(tc.flags.lib, {{"name", token.substr(2)}});
        } else {
            result.libs += " " + token;
        }
    }
}

std::optional<std::string> readFileToString(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string_view trim(std::string_view text) {
    size_t begin = text.find_first_not_of(" \t\r");
    if (begin == std::string_view::npos) {
        return {};
    }
    size_t end = text.find_last_not_of(" \t\r");
    return text.substr(begin, end - begin + 1);
}

// Reduces a single `Depends:` entry to its bare package name by stripping any
// feature (`pkg[feat]`) or platform (`pkg (windows)`) qualifier and whitespace.
std::string normalizeDepToken(std::string_view token) {
    token = trim(token);
    if (size_t cut = token.find_first_of("[("); cut != std::string_view::npos) {
        token = token.substr(0, cut);
    }
    return std::string(trim(token));
}

std::optional<fs::path>
packageInfoList(const fs::path &installed_root, const std::string &name, const std::string &triplet) {
    const fs::path info_dir = installed_root / "vcpkg" / "info";
    std::error_code ec;
    if (!fs::is_directory(info_dir, ec)) {
        return std::nullopt;
    }

    const std::string prefix = name + "_";
    const std::string suffix = "_" + triplet + ".list";
    for (const fs::directory_entry &entry : fs::directory_iterator(info_dir, ec)) {
        const std::string filename = entry.path().filename().string();
        if (entry.is_regular_file(ec) && filename.starts_with(prefix) && filename.ends_with(suffix)) {
            return entry.path();
        }
    }
    return std::nullopt;
}

std::vector<fs::path>
packageFiles(const fs::path &installed_root, const std::string &name, const std::string &triplet) {
    std::vector<fs::path> result;
    const auto list_path = packageInfoList(installed_root, name, triplet);
    if (!list_path) {
        return result;
    }

    std::ifstream input(*list_path);
    for (std::string relative_path; std::getline(input, relative_path);) {
        if (!relative_path.empty() && !relative_path.ends_with('/')) {
            result.push_back(installed_root / relative_path);
        }
    }
    return result;
}

// True when the package has a real installed footprint for @p triplet, as
// opposed to a build-time helper port that only ships metadata under share/.
bool packageHasArtifacts(const fs::path &installed_root, const std::string &name, const std::string &triplet) {
    const std::string include_prefix = triplet + "/include/";
    const std::string lib_prefix = triplet + "/lib/";
    for (const fs::path &path : packageFiles(installed_root, name, triplet)) {
        const std::string relative = fs::relative(path, installed_root).generic_string();
        if (relative.starts_with(include_prefix) || relative.starts_with(lib_prefix)) {
            return true;
        }
    }
    return false;
}

// The library-file extensions a vcpkg package may ship, derived from the
// toolchain (falling back to platform defaults).
std::vector<std::string> libExtensions(const catalyst::toolchain::ToolchainDef &tc) {
    return tc.extensions.library_scan;
}

// Strips the toolchain's library prefix (e.g. "lib") from a library file stem.
std::string strippedLibStem(std::string stem, const catalyst::toolchain::ToolchainDef &tc) {
    if (!tc.extensions.static_lib_prefix.empty() && stem.starts_with(tc.extensions.static_lib_prefix)) {
        return stem.substr(tc.extensions.static_lib_prefix.size());
    }
    if (!tc.extensions.shared_lib_prefix.empty() && stem.starts_with(tc.extensions.shared_lib_prefix)) {
        return stem.substr(tc.extensions.shared_lib_prefix.size());
    }
    if (stem.starts_with("lib")) {
        return stem.substr(3);
    }
    return stem;
}

// Appends library files owned by one installed package. The vcpkg info list is
// used instead of scanning the shared triplet lib directory, which also
// contains artifacts from unrelated packages.
void appendScannedLibs(std::string &libs,
                       const fs::path &installed_root,
                       const std::string &name,
                       const std::string &triplet,
                       const catalyst::toolchain::ToolchainDef &tc) {
    std::vector<std::string> extensions = libExtensions(tc);
    const fs::path release_lib_dir = installed_root / triplet / "lib";
    for (const fs::path &path : packageFiles(installed_root, name, triplet)) {
        if (path.parent_path() != release_lib_dir) {
            continue;
        }
        std::string file_ext = path.extension().string();
        if (std::ranges::find(extensions, file_ext) == extensions.end()) {
            continue;
        }
        std::string stem = strippedLibStem(path.stem().string(), tc);
        libs += " " + catalyst::toolchain::expandTemplate(tc.flags.lib, {{"name", stem}});
        catalyst::logger.debug("Found and added library: {}", stem);
    }
}

// Resolves the link flags contributed by a single vcpkg package. Prefers the
// package's pkg-config file; otherwise scans its lib/ directory for archives /
// shared objects and converts each into the toolchain's library token.
// @p fabricate_if_missing controls whether a bare `-l<name>` is emitted when the
// package has no lib/ directory at all (used only for an explicitly requested
// dependency, never for auto-discovered transitive ones).
struct ResolvedVcpkgPackage {
    FindRes flags;
    bool pkg_config_expanded_dependencies = false;
};

ResolvedVcpkgPackage resolveVcpkgPackage(const fs::path &installed_root,
                                         const std::string &name,
                                         const std::string &triplet,
                                         const std::string &linkage,
                                         const catalyst::toolchain::ToolchainDef &tc,
                                         bool fabricate_if_missing,
                                         bool use_pkg_config) {
    fs::path lib_path = installed_root / triplet / "lib";

    fs::path pkg_config_dir = lib_path / "pkgconfig";
    fs::path pc_file = pkg_config_dir / std::format("{}.pc", name);

    if (use_pkg_config && fs::exists(pc_file)) {
        catalyst::logger.debug("Found pkg-config file for {}: {}", name, pc_file.string());

        std::unordered_map<std::string, std::string> env;
        if (const char *path_env = std::getenv("PATH")) {
            env["PATH"] = path_env;
        }
        env["PKG_CONFIG_PATH"] = pkg_config_dir.string();

        auto link_dirs_res = processExecStdout({"pkg-config", "--libs-only-L", name}, std::nullopt, env);
        auto link_libs_res =
            processExecStdout({"pkg-config", "--libs-only-l", "--libs-only-other", name}, std::nullopt, env);

        if (link_dirs_res && link_libs_res) {
            std::string link_dirs = *link_dirs_res;
            std::string link_libs = *link_libs_res;

            if (auto last = link_dirs.find_last_not_of(" \t\n"); last != std::string::npos)
                link_dirs.erase(last + 1);
            if (auto last = link_libs.find_last_not_of(" \t\n"); last != std::string::npos)
                link_libs.erase(last + 1);

            catalyst::logger.debug("Resolved via pkg-config: L='{}' l='{}'", link_dirs, link_libs);
            FindRes result{.lib_path = "", .inc_path = "", .libs = "", .lib_dirs = {}};
            appendPkgConfigLibs(result, link_dirs, link_libs, tc);
            return {.flags = std::move(result), .pkg_config_expanded_dependencies = true};
        }
        catalyst::logger.warn("pkg-config failed for {}, falling back.", name);
    }
    catalyst::logger.debug("Did not find pkg-config file for {}: {}", name, pc_file.string());

    std::string library_path;
    std::string libs;
    std::vector<std::string> lib_dirs;

    const bool has_artifacts = packageHasArtifacts(installed_root, name, triplet);
    if ((linkage == "static" || linkage == "shared") && !has_artifacts) {
        catalyst::logger.warn(
            "Could not find installed artifacts for vcpkg package '{}' under: {}", name, installed_root.string());
        if (fabricate_if_missing) {
            libs += " " + catalyst::toolchain::expandTemplate(tc.flags.lib, {{"name", name}});
        }
    }

    library_path += " " + catalyst::toolchain::expandTemplate(tc.flags.lib_dir, {{"path", lib_path.string()}});
    catalyst::logger.debug("Adding library path: {}", lib_path.string());
    lib_dirs.push_back(lib_path.string());

    if ((linkage == "static" || linkage == "shared") && has_artifacts) {
        appendScannedLibs(libs, installed_root, name, triplet, tc);
    }

    return {.flags = {.lib_path = library_path, .inc_path = "", .libs = libs, .lib_dirs = lib_dirs},
            .pkg_config_expanded_dependencies = false};
}
} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::string vcpkgStatusKey(std::string_view name, std::string_view triplet) {
    std::string key(name);
    key.push_back('\x1f');
    key.append(triplet);
    return key;
}

VcpkgStatusDb parseVcpkgStatus(std::string_view status_content) {
    VcpkgStatusDb db;

    std::string package;
    std::string architecture;
    std::string status;
    std::string depends_line;

    auto flush = [&]() {
        const bool installed =
            status.find("installed") != std::string::npos && status.find("not-installed") == std::string::npos;
        if (!package.empty() && !architecture.empty() && installed) {
            std::vector<std::string> &deps = db.depends[vcpkgStatusKey(package, architecture)];
            std::stringstream tokens(depends_line);
            std::string raw;
            while (std::getline(tokens, raw, ',')) {
                std::string dep = normalizeDepToken(raw);
                if (!dep.empty() && std::ranges::find(deps, dep) == deps.end()) {
                    deps.push_back(dep);
                }
            }
        }
        package.clear();
        architecture.clear();
        status.clear();
        depends_line.clear();
    };

    size_t pos = 0;
    while (pos <= status_content.size()) {
        size_t newline = status_content.find('\n', pos);
        std::string_view line =
            status_content.substr(pos, (newline == std::string_view::npos ? status_content.size() : newline) - pos);

        if (trim(line).empty()) {
            flush();
        } else if (size_t colon = line.find(':'); colon != std::string_view::npos) {
            std::string_view key = trim(line.substr(0, colon));
            std::string_view value = trim(line.substr(colon + 1));
            if (key == "Package") {
                package = std::string(value);
            } else if (key == "Architecture") {
                architecture = std::string(value);
            } else if (key == "Status") {
                status = std::string(value);
            } else if (key == "Depends") {
                depends_line = std::string(value);
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        pos = newline + 1;
    }
    flush(); // final stanza when the file has no trailing blank line

    return db;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::vector<std::string>
vcpkgTransitiveClosure(const VcpkgStatusDb &db,
                       const std::string &root_name,
                       const std::string &triplet,
                       const std::function<bool(const std::string &name, const std::string &triplet)> &is_real_port) {
    std::vector<std::string> ordered;
    std::unordered_set<std::string> visited;

    // Post-order DFS: a package is appended only after all packages it depends
    // on. Reversing the result yields a static-link order in which every
    // package precedes the packages it depends on.
    std::function<void(const std::string &)> visit = [&](const std::string &node) {
        if (!visited.insert(node).second) {
            return;
        }
        if (auto it = db.depends.find(vcpkgStatusKey(node, triplet)); it != db.depends.end()) {
            for (const std::string &child : it->second) {
                if (is_real_port(child, triplet)) {
                    visit(child);
                }
            }
        }
        ordered.push_back(node);
    };
    visit(root_name);

    std::ranges::reverse(ordered);
    if (!ordered.empty() && ordered.front() == root_name) {
        ordered.erase(ordered.begin());
    }
    return ordered;
}

Result<FindRes>
findVcpkg(const std::string &build_dir, ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc) {
    namespace yaml = catalyst::utils::yaml;
    std::string dep_name = yaml::asString(yaml::child(dep, "name")).value_or("<unnamed>");
    auto triplet_opt = yaml::asString(yaml::child(dep, "triplet"));
    if (!triplet_opt) {
        return std::unexpected(std::format("vcpkg dependency '{}' does not define a triplet.", dep_name));
    }
    const std::string &triplet = *triplet_opt;
    std::string linkage = yaml::asString(yaml::child(dep, "linkage")).value_or("shared");
    bool transitive = yaml::asBool(yaml::child(dep, "transitive")).value_or(true);

    fs::path installed_root = fs::absolute(fs::path(build_dir) / "vcpkg_installed");
    if (!fs::is_directory(installed_root)) {
        const char *vcpkg_root_env = std::getenv("VCPKG_ROOT");
        if (vcpkg_root_env == nullptr) {
            return std::unexpected(
                std::format("No manifest install exists and VCPKG_ROOT is not set for '{}'.", dep_name));
        }
        installed_root = fs::path(vcpkg_root_env) / "installed";
        catalyst::logger.warn("Using the legacy global vcpkg installed tree for '{}'. Run 'catalyst fetch' to create "
                              "the profile-local manifest installation.",
                              dep_name);
    }

    catalyst::logger.debug("Resolving vcpkg dependency: {}", dep_name);
    ResolvedVcpkgPackage root = resolveVcpkgPackage(installed_root,
                                                    dep_name,
                                                    triplet,
                                                    linkage,
                                                    tc,
                                                    /*fabricate_if_missing=*/true,
                                                    /*use_pkg_config=*/transitive);
    FindRes result = std::move(root.flags);
    result.inc_path += " "
                       + catalyst::toolchain::expandTemplate(
                           tc.flags.include_dir, {{"path", (installed_root / triplet / "include").string()}});

    if (!transitive || root.pkg_config_expanded_dependencies) {
        return result;
    }

    // Discover transitive vcpkg dependencies from the installed-package database
    // and resolve each the same way. vcpkg packages each dependency separately
    // (e.g. ryml depends on c4core), so without this the user must list every
    // transitive package by hand. Build-time helper ports are pruned by the
    // packageHasArtifacts predicate, and packages are emitted parent-before-child
    // so static link order stays correct.
    fs::path status_path = installed_root / "vcpkg" / "status";
    auto status_content = readFileToString(status_path);
    if (!status_content) {
        catalyst::logger.warn("Could not read vcpkg status database at {}; skipping transitive resolution for '{}'.",
                              status_path.string(),
                              dep_name);
        return result;
    }

    VcpkgStatusDb db = parseVcpkgStatus(*status_content);
    auto is_real_port = [&installed_root](const std::string &name, const std::string &tr) {
        return packageHasArtifacts(installed_root, name, tr);
    };

    for (const std::string &tdep : vcpkgTransitiveClosure(db, dep_name, triplet, is_real_port)) {
        catalyst::logger.debug("Resolving transitive vcpkg dependency of {}: {}", dep_name, tdep);
        ResolvedVcpkgPackage resolved = resolveVcpkgPackage(installed_root,
                                                            tdep,
                                                            triplet,
                                                            linkage,
                                                            tc,
                                                            /*fabricate_if_missing=*/false,
                                                            /*use_pkg_config=*/false);
        result.lib_path += resolved.flags.lib_path;
        result.libs += resolved.flags.libs;
        result.lib_dirs.insert(result.lib_dirs.end(), resolved.flags.lib_dirs.begin(), resolved.flags.lib_dirs.end());
    }

    return result;
}
} // namespace catalyst::generate
