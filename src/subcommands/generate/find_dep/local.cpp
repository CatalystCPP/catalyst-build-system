#include <expected>
#include <format>
#include <string>

#include "catalyst/dir_guard.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::generate {
namespace fs = std::filesystem;

namespace {
Result<FindRes> resolveLocal(ryml::ConstNodeRef dep,
                             const catalyst::toolchain::ToolchainDef &tc,
                             std::unordered_set<fs::path> ancestors) {
    namespace yaml = catalyst::utils::yaml;
    std::string dep_name = yaml::asString(yaml::child(dep, "name")).value_or("<unnamed>");
    catalyst::logger.debug("Resolving local dependency: {}", dep_name);

    auto dep_path_opt = yaml::asString(yaml::child(dep, "path"));
    if (!dep_path_opt) {
        return std::unexpected(std::format("Local Dependency: {} does not define path.", dep_name));
    }

    std::error_code ec;
    fs::path dep_path = fs::canonical(*dep_path_opt, ec);
    if (ec)
        return std::unexpected(std::format("Cannot resolve local dependency '{}': {}", dep_name, ec.message()));
    if (!ancestors.insert(dep_path).second)
        return std::unexpected(std::format("Dependency cycle detected involving {}", dep_path.string()));
    catalyst::logger.debug("Changing directory to: {}", dep_path.string());
    catalyst::DirectoryChangeGuard dg(dep_path);

    std::vector<std::string> profiles =
        yaml::asStringVector(yaml::child(dep, "profiles")).value_or(std::vector<std::string>{});
    if (profiles.empty())
        profiles.emplace_back("common");

    catalyst::logger.debug("Composing profiles for local dependency.");
    auto pc = catalyst::generate::profileComposition(profiles);

    if (!pc) {
        return std::unexpected(pc.error());
    }
    const utils::yaml::Configuration &profile = pc.value();

    auto features = resolveFeatureFlags(
        profile, yaml::asStringVector(yaml::child(dep, "using")).value_or(std::vector<std::string>{}));
    if (!features)
        return std::unexpected(std::format("Local dependency '{}': {}", dep_name, features.error()));
    auto definitions = featureDefinitions(profile.getString("manifest.name").value_or("name"), *features);

    // Resolution is read-only; fetch builds the same profiles and feature overrides.

    // Add include directories
    std::string include_path;
    if (auto includes = profile.getStringVector("manifest.dirs.include")) {
        for (const auto &dir : *includes) {
            auto curr = fs::absolute(dir);
            catalyst::logger.debug("Adding include path: {}", curr.string());
            include_path += " " + catalyst::toolchain::expandTemplate(tc.flags.include_dir, {{"path", curr.string()}});
        }
    }

    // Add library directory
    std::string library_path;
    std::vector<std::string> lib_dirs;
    if (auto build_dir_str = profile.getString("manifest.dirs.build")) {
        fs::path build_dir = catalyst::utils::yaml::multiplexedBuildDir(*build_dir_str, profiles);
        auto lib_path = fs::absolute(build_dir);
        catalyst::logger.debug("Adding library path: {}", lib_path.string());
        library_path += " " + catalyst::toolchain::expandTemplate(tc.flags.lib_dir, {{"path", lib_path.string()}});
        lib_dirs.push_back(lib_path.string());
    }

    // Add library
    std::string libs;
    std::string type = profile.getString("manifest.type").value_or("BINARY");
    if (type != "INTERFACE") {
        if (auto lib_name = profile.getString("manifest.name")) {
            catalyst::logger.debug("Adding library: {}", *lib_name);
            libs += " " + catalyst::toolchain::expandTemplate(tc.flags.lib, {{"name", *lib_name}});
        }
    }

    FindRes result{.lib_path = library_path,
                   .inc_path = include_path,
                   .libs = libs,
                   .lib_dirs = lib_dirs,
                   .definitions = std::move(definitions)};
    if (auto deps = yaml::child(profile.rootRef(), "dependencies"); deps.readable() && deps.is_seq()) {
        for (auto child : deps.children()) {
            auto resolved = yaml::asString(yaml::child(child, "source")) == "local"
                                ? resolveLocal(child, tc, ancestors)
                                : findDep(profile.getBuildDir().string(), child, tc);
            if (!resolved)
                return std::unexpected(resolved.error());
            auto merged = mergeFeatureDefinitions(result.definitions, resolved->definitions);
            if (!merged)
                return std::unexpected(merged.error());
            result.definitions = std::move(*merged);
            result.inc_path += " " + resolved->inc_path;
            result.lib_path += " " + resolved->lib_path;
            result.libs += " " + resolved->libs;
            result.lib_dirs.insert(result.lib_dirs.end(), resolved->lib_dirs.begin(), resolved->lib_dirs.end());
        }
    }
    return result;
}
} // namespace

Result<FindRes> findLocal(ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc) {
    try {
        return resolveLocal(dep, tc, {fs::canonical(fs::current_path())});
    } catch (const std::exception &error) {
        return std::unexpected(error.what());
    }
}
} // namespace catalyst::generate
