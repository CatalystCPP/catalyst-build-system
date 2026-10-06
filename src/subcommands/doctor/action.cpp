#include "catalyst/subcommands/doctor.hpp"

#ifndef _WIN32
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>

#include <ryml/ryml.hpp>
#include <ryml/ryml_std.hpp>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/toolchain.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"
#include "catalyst/workspace.hpp"

namespace catalyst::doctor {
namespace {
namespace fs = std::filesystem;
namespace yaml = catalyst::utils::yaml;

struct Check {
    std::string id;
    std::string status;
    std::string message;
    std::string hint;
};

struct Report {
    std::vector<Check> checks;

    void add(std::string_view id, std::string_view status, std::string_view message, std::string_view hint = {}) {
        checks.push_back({std::string{id}, std::string{status}, std::string{message}, std::string{hint}});
    }

    [[nodiscard]] std::size_t count(std::string_view status) const {
        return std::ranges::count_if(checks, [status](const Check &check) { return check.status == status; });
    }
};

// Look up literal executable names, never interpolate manifest values into a shell.
bool executableFile(const fs::path &path) {
    std::error_code error;
    if (!fs::is_regular_file(path, error))
        return false;
#ifdef _WIN32
    return true;
#else
    return ::access(path.c_str(), X_OK) == 0;
#endif
}

[[nodiscard]] std::optional<fs::path> findExecutable(std::string_view executable) {
    if (executable.empty())
        return std::nullopt;
    const fs::path requested{executable};
    if (requested.has_parent_path())
        return executableFile(requested) ? std::optional{fs::absolute(requested)} : std::nullopt;
    const char *path = std::getenv("PATH");
    if (!path)
        return std::nullopt;
#ifdef _WIN32
    constexpr char PATH_SEPARATOR = ';';
#else
    constexpr char PATH_SEPARATOR = ':';
#endif
    for (const auto entry : std::views::split(std::string_view{path}, PATH_SEPARATOR)) {
        const fs::path directory{std::string_view{entry}};
        const fs::path candidate = (directory.empty() ? fs::path{"."} : directory) / requested;
        if (executableFile(candidate))
            return fs::absolute(candidate);
#ifdef _WIN32
        for (const auto suffix : {".exe", ".cmd", ".bat"}) {
            const fs::path suffixed{candidate.string() + suffix};
            if (executableFile(suffixed))
                return fs::absolute(suffixed);
        }
#endif
    }
    return std::nullopt;
}

void checkTool(Report &report, std::string_view id, std::string_view executable) {
    if (auto found = findExecutable(executable)) {
        report.add(id, "ok", std::format("{}: {}", executable, found->string()));
    } else {
        report.add(id,
                   "error",
                   std::format("Executable '{}' is unavailable or not executable", executable),
                   "Install the tool, add it to PATH, or correct the configured executable path.");
    }
}

bool checkProfileShape(Report &report, ryml::ConstNodeRef profile, std::string_view name) {
    bool valid = true;
    for (const auto *key : {"meta", "manifest", "hooks", "dependencies", "features"}) {
        const auto section = yaml::child(profile, key);
        if (!section.readable() || (section.has_val() && section.val_is_null()))
            continue; // null deliberately unsets inherited sections.
        const bool sequence = std::string_view{key} == "dependencies";
        const bool flexible = std::string_view{key} == "features";
        if ((sequence && !section.is_seq()) || (!sequence && !section.is_map() && !(flexible && section.is_seq()))) {
            report.add("profiles.shape",
                       "error",
                       std::format("Profile '{}': '{}' has the wrong structure", name, key),
                       sequence ? "Use a sequence of dependency mappings."
                                : "Use a mapping (features also accepts a sequence of mappings).");
            valid = false;
        }
    }
    const auto meta = yaml::child(profile, "meta");
    if (auto generator = yaml::asString(yaml::child(meta, "generator"));
        generator && *generator != "cob" && *generator != "ninja" && *generator != "make" && *generator != "gmake")
        report.add("build.backend",
                   "error",
                   std::format("Profile '{}': unsupported backend '{}'", name, *generator),
                   "Use cob, ninja, gmake, or make. Invalid declarations are ignored during composition.");
    const auto manifest = yaml::child(profile, "manifest");
    if (auto type = yaml::asString(yaml::child(manifest, "type"));
        type && *type != "BINARY" && *type != "STATICLIB" && *type != "SHAREDLIB" && *type != "INTERFACE")
        report.add(
            "manifest.type",
            "error",
            std::format("Profile '{}': unsupported artifact type '{}'", name, *type),
            "Use BINARY, STATICLIB, SHAREDLIB, or INTERFACE. Invalid declarations are ignored during composition.");
    return valid;
}

// Validate the selected files before composing: composition expects mappings.
bool checkProfiles(Report &report, std::span<const std::string> profiles) {
    std::optional<ryml::Tree> centralized;
    if (fs::exists("CATALYST.yaml")) {
        auto loaded = yaml::loadFile("CATALYST.yaml");
        if (!loaded || !loaded->crootref().is_map()) {
            report.add("profiles.manifest",
                       "error",
                       loaded ? "CATALYST.yaml must be a mapping" : loaded.error(),
                       "Fix the centralized manifest before composing profiles.");
            return false;
        }
        centralized = std::move(*loaded);
    }
    bool valid = true;
    for (const auto &name : profiles) {
        const auto selected = centralized ? yaml::child(centralized->crootref(), name) : ryml::ConstNodeRef{};
        const fs::path split = name == "common" ? "catalyst.yaml" : std::format("catalyst_{}.yaml", name);
        if (selected.readable()) {
            if (!selected.is_map()) {
                report.add("profiles.shape", "error", std::format("Profile '{}' must be a mapping", name));
                valid = false;
            } else {
                if (!checkProfileShape(report, selected, name))
                    valid = false;
                report.add("profiles.selected", "ok", std::format("Profile '{}' from CATALYST.yaml", name));
                if (fs::exists(split))
                    report.add("profiles.shadowed",
                               "warning",
                               std::format("{} is shadowed by CATALYST.yaml", split.string()),
                               "Edit the centralized profile; the split file is not used for this composition.");
            }
            continue;
        }
        auto loaded = yaml::loadFile(split);
        if (!loaded || !loaded->crootref().is_map()) {
            report.add("profiles.selected",
                       "error",
                       loaded ? std::format("{} must be a mapping", split.string()) : loaded.error(),
                       "Create the requested profile or correct its YAML.");
            valid = false;
        } else {
            if (!checkProfileShape(report, loaded->crootref(), name))
                valid = false;
            report.add("profiles.selected", "ok", std::format("Profile '{}' from {}", name, split.string()));
        }
    }
    return valid;
}

void checkDirectories(Report &report, const yaml::Configuration &config, std::string_view kind) {
    const std::string key = std::format("manifest.dirs.{}", kind);
    auto directories = config.getStringVector(key);
    if (!directories) {
        report.add(key, "error", key + " must be a sequence of directory names");
        return;
    }
    const bool required = kind == "source" && config.getString("manifest.type").value_or("BINARY") != "INTERFACE";
    if (directories->empty() && required)
        report.add(key,
                   "warning",
                   "No source directories are configured",
                   "Set manifest.dirs.source, or use type INTERFACE for a header-only target.");
    for (const auto &directory : *directories) {
        std::error_code error;
        if (fs::is_directory(directory, error))
            report.add(key, "ok", std::format("Directory exists: {}", directory));
        else
            report.add(
                key,
                required ? "error" : "warning",
                std::format("Directory unavailable: {}{}", directory, error ? ": " + error.message() : ""),
                "Correct the path, create the directory, or ensure a generation hook creates it before building.");
    }
}

void checkBuildDirectory(Report &report, const yaml::Configuration &config) {
    const fs::path build = config.getBuildDir();
    // Do not create a probe file or directory just to check readiness.
    fs::path ancestor = fs::absolute(build).lexically_normal();
    while (!fs::exists(ancestor) && ancestor.has_relative_path())
        ancestor = ancestor.parent_path();
    if (!fs::is_directory(ancestor)) {
        report.add("build.directory",
                   "error",
                   std::format("Build path is blocked by a non-directory: {}", ancestor.string()),
                   "Choose a different manifest.dirs.build or remove the blocking file.");
        return;
    }
#ifndef _WIN32
    if (::access(ancestor.c_str(), W_OK | X_OK) != 0) {
        report.add("build.directory",
                   "error",
                   std::format("Build directory ancestor is not writable: {}", ancestor.string()),
                   "Choose a writable build directory or correct directory permissions.");
        return;
    }
#endif
    report.add("build.directory", "ok", std::format("Build destination: {}", build.string()));
    if (!fs::exists(build / "compile_commands.json"))
        report.add("build.compile_commands",
                   "warning",
                   "No compilation database for this composition yet",
                   "Run catalyst build with these profiles to generate compile_commands.json for IDE tooling.");
    else
        report.add("build.compile_commands", "ok", "Compilation database is present (freshness not checked)");
}

void checkLocalDependency(Report &report, ryml::ConstNodeRef dependency, std::string_view name) {
    const auto path = yaml::asString(yaml::child(dependency, "path"));
    if (!path || path->empty())
        return; // The caller reports missing required fields.
    if (!fs::is_directory(*path))
        report.add("dependencies.local",
                   "error",
                   std::format("Local dependency '{}' directory is unavailable: {}", name, *path),
                   "Correct the path or check out the local dependency.");
    else if (!fs::exists(fs::path{*path} / "CATALYST.yaml") && !fs::exists(fs::path{*path} / "catalyst.yaml"))
        report.add("dependencies.local", "error", std::format("Local dependency '{}' has no Catalyst manifest", name));
    else
        report.add("dependencies.local",
                   "ok",
                   std::format("Local dependency '{}' is present (not recursively validated)", name));
}

void checkDependencies(Report &report, const yaml::Configuration &config) {
    const auto dependencies = yaml::child(config.rootRef(), "dependencies");
    if (!dependencies.readable())
        return;
    if (!dependencies.is_seq()) {
        report.add("dependencies.shape", "error", "dependencies must be a sequence");
        return;
    }
    std::unordered_set<std::string> tools;
    bool remote = false;
    for (const auto dependency : dependencies.children()) {
        const auto name = yaml::asString(yaml::child(dependency, "name"));
        const auto source = yaml::asString(yaml::child(dependency, "source"));
        if (!dependency.is_map() || !name || name->empty() || !source) {
            report.add("dependencies.declaration", "error", "Each dependency requires a nonempty name and a source");
            continue;
        }
        auto require_field = [&report, dependency, &name](std::string_view field) {
            auto value = yaml::asString(yaml::child(dependency, field));
            if (!value || value->empty())
                report.add(
                    "dependencies.declaration", "error", std::format("Dependency '{}' requires '{}'", *name, field));
        };
        if (*source == "local") {
            require_field("path");
            checkLocalDependency(report, dependency, *name);
        } else if (*source == "git") {
            require_field("url");
            require_field("version");
            tools.insert("git");
            remote = true;
        } else if (*source == "vcpkg") {
            require_field("triplet");
            tools.insert("vcpkg");
            remote = true;
        } else if (*source == "conan") {
            require_field("version");
            tools.insert("conan");
            tools.insert("pkg-config");
            remote = true;
        } else if (*source == "system") {
            const auto lib = yaml::asString(yaml::child(dependency, "lib"));
            const auto include = yaml::asString(yaml::child(dependency, "include"));
            if (lib && !fs::exists(*lib))
                report.add("dependencies.system",
                           "error",
                           std::format("System library path '{}' is missing: {}", *name, *lib));
            if (include && !fs::is_directory(*include))
                report.add("dependencies.system",
                           "error",
                           std::format("System include path '{}' is missing: {}", *name, *include));
            if (!lib || !include)
                tools.insert("pkg-config");
        } else if (*source == "custom") {
            if (!yaml::asString(yaml::child(dependency, "command"))
                && !yaml::asString(yaml::child(dependency, "script")))
                report.add("dependencies.custom",
                           "error",
                           std::format("Custom dependency '{}' requires command or script", *name));
            else
                report.add("dependencies.custom",
                           "warning",
                           std::format("Custom dependency '{}' is not executed by doctor", *name),
                           "Verify its command or script separately; doctor never runs project code.");
        } else {
            report.add("dependencies.source",
                       "error",
                       std::format("Dependency '{}' has unsupported source '{}'", *name, *source),
                       "Use git, vcpkg, conan, local, system, or custom.");
        }
    }
    std::vector<std::string> sorted_tools{tools.begin(), tools.end()};
    std::ranges::sort(sorted_tools);
    for (const auto &tool : sorted_tools)
        checkTool(report, "dependencies.tool", tool);
    if (remote) {
        const auto workspace = catalyst::Workspace::findRoot();
        const fs::path lock = workspace ? workspace->getRoot() / "catalyst.lock" : fs::path{"catalyst.lock"};
        if (!fs::is_regular_file(lock))
            report.add("dependencies.lock",
                       "warning",
                       std::format("No lockfile at {}", lock.string()),
                       "Run catalyst lock to pin dependencies; doctor does not resolve or fetch them.");
        else
            report.add(
                "dependencies.lock", "ok", std::format("Lockfile present: {} (contents not verified)", lock.string()));
    }
}

void checkConfiguration(Report &report, const yaml::Configuration &config, const Parse &args) {
    const auto type = config.getString("manifest.type").value_or("BINARY");
    if (type != "BINARY" && type != "STATICLIB" && type != "SHAREDLIB" && type != "INTERFACE")
        report.add("manifest.type",
                   "error",
                   std::format("Unsupported artifact type '{}'", type),
                   "Use BINARY, STATICLIB, SHAREDLIB, or INTERFACE.");
    else
        report.add("manifest.type",
                   "ok",
                   std::format("{} target '{}'", type, config.getString("manifest.name").value_or("<unnamed>")));

    checkDirectories(report, config, "source");
    checkDirectories(report, config, "include");
    checkBuildDirectory(report, config);

    const std::string backend =
        args.backend.empty() ? config.getString("meta.generator").value_or("cob") : args.backend;
    if (backend == "ninja" || backend == "make" || backend == "gmake")
        checkTool(report, "build.backend", backend == "ninja" ? "ninja" : "make");
    else if (backend == "cob")
        report.add("build.backend", "ok", "COB backend is bundled with Catalyst");
    else
        report.add("build.backend",
                   "error",
                   std::format("Unsupported backend '{}'", backend),
                   "Use cob, ninja, gmake, or make.");

    std::optional<fs::path> toolchain_path;
    if (auto path = config.getString("manifest.toolchain"))
        toolchain_path = *path;
    auto resolved = catalyst::toolchain::resolveToolchain(toolchain_path);
    if (!resolved) {
        report.add("toolchain.resolve", "error", resolved.error(), "Correct the toolchain file and its extends chain.");
    } else {
        report.add("toolchain.resolve",
                   "ok",
                   std::format("Resolved toolchain '{}'{}",
                               resolved->name,
                               toolchain_path ? " from " + toolchain_path->string() : " (built-in defaults)"));
        if (type != "INTERFACE") {
            checkTool(report, "toolchain.c", resolved->compiler.c.executable);
            checkTool(report, "toolchain.cxx", resolved->compiler.cxx.executable);
            if (type == "STATICLIB")
                checkTool(report, "toolchain.archiver", resolved->archiver.executable);
            else
                checkTool(report, "toolchain.linker", resolved->linker.executable);
            for (const auto *key : {"manifest.tooling.CC_LAUNCHER", "manifest.tooling.CXX_LAUNCHER"})
                if (auto launcher = config.getString(key); launcher && !launcher->empty()) {
                    // Launchers may include arguments, unlike toolchain executable fields.
                    const auto end = launcher->find_first_of(" \t");
                    checkTool(report, "toolchain.launcher", launcher->substr(0, end));
                }
        }
    }
    auto features = catalyst::generate::resolveFeatureFlags(config, args.enabled_features);
    if (!features)
        report.add("features.resolve", "error", features.error(), "Correct feature declarations or -f overrides.");
    else
        report.add("features.resolve", "ok", std::format("Validated {} feature flags", features->size()));
    checkDependencies(report, config);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void setString(ryml::NodeRef parent, std::string_view key, std::string_view value) {
    auto node = yaml::childOrCreate(parent, key);
    node.set_val(parent.tree()->to_arena(yaml::toSubstr(value)));
    node |= ryml::VALQUO;
}

void printReport(const Report &report, const Parse &args, bool healthy) {
    if (!args.json) {
        std::println("Catalyst doctor");
        for (const auto &check : report.checks) {
            std::println("[{}] {}: {}", check.status, check.id, check.message);
            if (!check.hint.empty())
                std::println("  Fix: {}", check.hint);
        }
        std::println("\n{} ok, {} warnings, {} errors — {}{}",
                     report.count("ok"),
                     report.count("warning"),
                     report.count("error"),
                     healthy ? "PASS" : "FAIL",
                     args.strict ? " (strict)" : "");
        return;
    }
    ryml::Tree tree;
    auto root = tree.rootref();
    root |= ryml::MAP;
    root["schema_version"] << 1;
    setString(root, "root", fs::current_path().string());
    auto profiles = root["profiles"];
    profiles |= ryml::SEQ;
    for (const auto &profile : args.profiles) {
        auto value = profiles.append_child();
        value << profile;
        value |= ryml::VALQUO;
    }
    root["strict"].set_val(args.strict ? "true" : "false");
    root["healthy"].set_val(healthy ? "true" : "false");
    auto summary = root["summary"];
    summary |= ryml::MAP;
    for (const auto *status : {"ok", "warning", "error"})
        summary[yaml::toSubstr(status)] << report.count(status);
    auto checks = root["checks"];
    checks |= ryml::SEQ;
    for (const auto &check : report.checks) {
        auto item = checks.append_child();
        item |= ryml::MAP;
        setString(item, "id", check.id);
        setString(item, "status", check.status);
        setString(item, "message", check.message);
        setString(item, "hint", check.hint);
    }
    std::println("{}", ryml::emitrs_json<std::string>(tree));
}
} // namespace

Result<void> action(const Parse &args) {
    Report report;
    try {
        if (checkProfiles(report, args.profiles)) {
            yaml::Configuration config(args.profiles);
            checkConfiguration(report, config, args);
        }
    } catch (const std::exception &error) {
        report.add("configuration.inspect",
                   "error",
                   error.what(),
                   "Correct the configuration or filesystem problem and rerun doctor.");
    }
    const bool healthy = report.count("error") == 0 && (!args.strict || report.count("warning") == 0);
    printReport(report, args, healthy);
    if (!healthy)
        return std::unexpected("Doctor found readiness problems; see the diagnostic report.");
    return {};
}
} // namespace catalyst::doctor
