#include <sys/wait.h>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "catalyst/hooks.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::generate {
namespace fs = std::filesystem;

std::string buildFilename(std::string_view generator) {
    if (generator == "ninja")
        return "build.ninja";
    if (generator == "gmake" || generator == "make")
        return "Makefile";
    return "catalyst.build";
}

namespace {

bool isEnabled(bool default_enabled,
               const std::string &feature,
               const std::unordered_set<std::string> &enabled_features) = delete;

// A feature's default state: either `feature: <bool>` or `feature: {default: <bool>}`.
bool featureDefault(ryml::ConstNodeRef feature_node) = delete;

struct GeneratedVariables {
    std::string cxxflags;
    std::vector<std::string> rpaths;
    std::vector<std::string> link_inputs;
};

/// Writes the variable block and returns values needed by later build edges.
Result<GeneratedVariables> writeVariables(const catalyst::utils::yaml::Configuration &config,
                                          catalyst::generate::buildwriters::BaseWriter &writer,
                                          const std::vector<FeatureFlag> &features,
                                          const catalyst::toolchain::ToolchainDef &tc);
void writeRules(catalyst::generate::buildwriters::BaseWriter &writer, const catalyst::toolchain::ToolchainDef &tc);
Result<std::vector<std::string>> intermediateTargets(catalyst::generate::buildwriters::BaseWriter &writer,
                                                     const std::unordered_set<fs::path> &source_set,
                                                     const catalyst::toolchain::ToolchainDef &tc,
                                                     const modules::ScanResult &module_scan);
void finalTarget(const utils::yaml::Configuration &config,
                 const auto &object_files,
                 catalyst::generate::buildwriters::BaseWriter &writer,
                 const catalyst::toolchain::ToolchainDef &tc,
                 const std::vector<std::string> &extra_link_args,
                 const std::vector<std::string> &link_inputs);

void featureFilter(
    std::unordered_set<fs::path> &source_set,
    const std::vector<FeatureFlag> &features); ///< drop requirement for config struct bc FeatureFlag is self contained

} // namespace

Result<void> action(const Parse &parse_args) {
    catalyst::logger.debug("Generate subcommand invoked.");

    catalyst::logger.debug("Composing profiles.");
    utils::yaml::Configuration config;

    try {
        config = utils::yaml::Configuration(parse_args.profiles);
    } catch (std::runtime_error &err) {
        return std::unexpected(err.what());
    }

    auto features_res = resolveFeatureFlags(config, parse_args.enabled_features);
    if (!features_res) {
        return std::unexpected(features_res.error());
    }
    const std::vector<FeatureFlag> &features = *features_res;

    if (!parse_args.skip_pre_generate) {
        catalyst::logger.debug("Running pre-generate hooks.");
        if (auto res = hooks::preGenerate(config); !res)
            return res;
    }

    fs::path current_dir = fs::current_path();
    std::vector<std::string> relative_source_dirs;
    std::vector<std::string> absolute_source_dirs;
    auto source_dirs_res = config.getStringVector("manifest.dirs.source");
    if (!source_dirs_res) {
        return std::unexpected("Unable to get value for manifest.dirs.source");
    }
    relative_source_dirs = source_dirs_res.value();
    absolute_source_dirs.reserve(relative_source_dirs.size());
    for (const auto &dir : relative_source_dirs)
        absolute_source_dirs.push_back((current_dir / dir).string());

    catalyst::logger.debug("Building source set.");
    auto source_set_res = buildSourceSet(absolute_source_dirs, parse_args.profiles);
    if (!source_set_res) {
        return std::unexpected(source_set_res.error());
    }

    std::unordered_set<std::filesystem::path> source_set = source_set_res.value();
    featureFilter(source_set, features);

    fs::path build_dir = config.getBuildDir();
    fs::path obj_dir = build_dir / "obj";

    catalyst::logger.debug("Creating object directory: {}", obj_dir.string());
    fs::create_directories(obj_dir);
    if (!fs::exists(obj_dir) || !fs::is_directory(obj_dir)) {
        return std::unexpected("Failed to create object directory: " + obj_dir.string());
    }

    std::string generator = parse_args.backend;
    if (generator.empty()) {
        generator = config.getString("meta.generator").value_or("cob");
    }

    const std::string build_filename = buildFilename(generator);

    const fs::path buildfile_path = build_dir / build_filename;
    std::optional<fs::path> toolchain_path;
    if (auto configured_path = config.getString("manifest.toolchain"))
        toolchain_path = *configured_path;
    auto resolved_toolchain = catalyst::toolchain::resolveToolchain(toolchain_path);
    if (!resolved_toolchain)
        return std::unexpected(resolved_toolchain.error());
    catalyst::toolchain::ToolchainDef tc = std::move(*resolved_toolchain);

    auto state = generationState(config, parse_args.enabled_features);
    if (!state)
        return std::unexpected(state.error());
    // Never treat a partially written build file as current after a failed generation.
    fs::remove(build_dir / GENERATION_STATE_FILENAME);

    catalyst::logger.debug("Writing build file to: {}", buildfile_path.string());
    std::ofstream buildfile{buildfile_path};
    if (!buildfile) {
        return std::unexpected(std::format("Failed to open {} for writing", buildfile_path.string()));
    }

    const bool uses_cob_writer = generator != "ninja" && generator != "gmake" && generator != "make";
    auto generate_build = [&](buildwriters::BaseWriter &writer) -> Result<void> {
        writer.addComment("Build file generated by Catalyst");
        auto variables = writeVariables(config, writer, features, tc);
        if (!variables)
            return std::unexpected(variables.error());
        writeRules(writer, tc);
        auto module_scan = modules::scanModules(source_set, tc, variables->cxxflags);
        if (!module_scan) {
            return std::unexpected(module_scan.error());
        }
        auto object_files = intermediateTargets(writer, source_set, tc, *module_scan);
        if (!object_files) {
            return std::unexpected(object_files.error());
        }
        const std::vector<std::string> extra_link_args =
            uses_cob_writer ? std::move(variables->rpaths) : std::vector<std::string>{};
        finalTarget(config, *object_files, writer, tc, extra_link_args, variables->link_inputs);
        return {};
    };

    Result<void> generate_res;
    if (generator == "ninja") {
        buildwriters::DerivedWriter<buildwriters::TargetType::Ninja> writer(buildfile);
        generate_res = generate_build(writer);
    } else if (generator == "gmake" || generator == "make") {
        buildwriters::DerivedWriter<buildwriters::TargetType::Make> writer(buildfile);
        generate_res = generate_build(writer);
    } else {
        buildwriters::DerivedWriter<buildwriters::TargetType::COB> writer(buildfile);
        generate_res = generate_build(writer);
    }
    if (!generate_res) {
        return generate_res;
    }
    buildfile.close();
    if (!buildfile) {
        return std::unexpected(std::format("Failed to write {}", buildfile_path.string()));
    }

    catalyst::logger.debug("Writing profile composition to: {}", (build_dir / "profile_composition.yaml").string());
    std::ofstream profile_comp_file{build_dir / "profile_composition.yaml"};
    if (!profile_comp_file) {
        return std::unexpected("Failed to open profile_composition.yaml for writing in " + build_dir.string());
    }
    profile_comp_file << utils::yaml::emitYaml(config.rootRef());

    catalyst::logger.debug("Running post-generate hooks.");
    if (auto res = hooks::postGenerate(config); !res) {
        return res;
    }

    const fs::path toolchain_store_path = build_dir / catalyst::toolchain::RESOLVED_TOOLCHAIN_STORE_FILENAME;
    catalyst::logger.debug("Writing resolved toolchain to: {}", toolchain_store_path.string());
    std::ofstream toolchain_store{toolchain_store_path, std::ios::binary | std::ios::trunc};
    if (!toolchain_store) {
        return std::unexpected(std::format("Failed to open {} for writing", toolchain_store_path.string()));
    }
    toolchain_store << catalyst::toolchain::serializeToolchainStore(tc, generator);
    toolchain_store.close();
    if (!toolchain_store) {
        return std::unexpected(std::format("Failed to write {}", toolchain_store_path.string()));
    }
    std::ofstream state_file{build_dir / GENERATION_STATE_FILENAME, std::ios::binary | std::ios::trunc};
    state_file << *state;
    state_file.close();
    if (!state_file)
        return std::unexpected("Failed to write generation state in " + build_dir.string());
    catalyst::logger.debug("Generate subcommand finished successfully.");
    return {};
}

namespace {
constexpr uint64_t fnv1aHash(const std::string &text) {
    constexpr uint64_t FNV1A_BIAS = 0xcbf29ce484222325ULL;
    constexpr uint64_t FNV1A_PRIME = 0x100000001b3ULL;
    uint64_t hash = FNV1A_BIAS;
    for (char c : text) {
        hash ^= static_cast<uint64_t>(c);
        hash *= FNV1A_PRIME;
    }
    return hash;
}

std::string
objectFilePath(const fs::path &src, const fs::path &current_dir, const catalyst::toolchain::ToolchainDef &tc) {
    fs::path relative_src_path = fs::relative(src, current_dir);
    std::string obj_name = std::format(
        "{}_{:016x}{}", relative_src_path.stem().string(), fnv1aHash(relative_src_path.string()), tc.extensions.object);
    return (fs::path{"obj"} / obj_name).string();
}

Result<std::vector<std::string>> intermediateTargets(catalyst::generate::buildwriters::BaseWriter &writer,
                                                     const std::unordered_set<std::filesystem::path> &source_set,
                                                     const catalyst::toolchain::ToolchainDef &tc,
                                                     const modules::ScanResult &module_scan) {
    catalyst::logger.debug("Generate subcommand invoked.");
    fs::path current_dir = fs::current_path();
    writer.addComment("Source File Compilation");
    std::vector<std::string> object_files;
    for (const auto &src : source_set) {
        object_files.push_back(objectFilePath(src, current_dir, tc));

        // Module treatment: an interface unit gets a -fmodule-output, an
        // importer gets -fmodule-file= plus an ordering edge on the
        // provider's *object* (the BMI is a compile side effect, not a
        // graph node).
        std::vector<std::string> extra_args;
        std::vector<std::string> implicit_deps;
        if (auto it = module_scan.tu_modules.find(src); it != module_scan.tu_modules.end()) {
            const modules::ModuleInfo &info = it->second;
            if (!info.provides.empty()) {
                if (modules::needsExplicitModuleType(src, tc)) {
                    extra_args.emplace_back("-x c++-module");
                }
                extra_args.push_back(std::format("-fmodule-output={}", modules::bmiPath(info.provides, tc)));
            }
            for (const auto &req : info.required) {
                auto provider = module_scan.providers.find(req);
                if (provider == module_scan.providers.end()) {
                    return std::unexpected(
                        std::format("No provider for module '{}' required by {}", req, src.string()));
                }
                extra_args.push_back(std::format("-fmodule-file={}={}", req, modules::bmiPath(req, tc)));
                implicit_deps.push_back(objectFilePath(provider->second, current_dir, tc));
            }
        }

        void(writer.addBuild(
            {object_files.back()},
            (std::ranges::contains(tc.extensions.c_sources, src.extension().string()) ? "cc_compile" : "cxx_compile"),
            {src.string()},
            implicit_deps,
            extra_args));
    }
    return object_files;
}

void finalTarget(const utils::yaml::Configuration &config,
                 const auto &object_files,
                 catalyst::generate::buildwriters::BaseWriter &writer,
                 const catalyst::toolchain::ToolchainDef &tc,
                 const std::vector<std::string> &extra_link_args,
                 const std::vector<std::string> &link_inputs) {
    std::string type = config.getString("manifest.type").value_or("BINARY");
    if (type == "INTERFACE") {
        catalyst::logger.debug("Interface library target, skipping final target build edge.");
        return;
    }
    catalyst::logger.debug("Generating final target.");
    // Build edge for the final target
    std::string target_prefix;
    std::string target_suffix;
    std::string link_rule;

    if (type == "STATICLIB") {
        link_rule = "static_link";
        target_prefix = tc.extensions.static_lib_prefix;
        target_suffix = tc.extensions.static_lib;
    } else if (type == "SHAREDLIB") {
        link_rule = "shared_link";
        target_prefix = tc.extensions.shared_lib_prefix;
        target_suffix = tc.extensions.shared_lib;
    } else { // BINARY or default
        link_rule = "binary_link";
        target_suffix = tc.extensions.executable;
    }

    std::string target_name = config.getString("manifest.name").value_or("name");
    fs::path target_path{target_prefix + target_name + target_suffix};
    catalyst::logger.debug("Final target name: {}", target_path.string());
    writer.addComment("Build edge for the final target");
    void(writer.addBuild({target_path.string()},
                         link_rule,
                         object_files,
                         link_rule == "static_link" ? std::vector<std::string>{} : link_inputs,
                         link_rule == "static_link" ? std::vector<std::string>{} : extra_link_args));

    // Default target
    writer.addComment("Default target to build");
    writer.addDefault(target_path.string());
}

Result<GeneratedVariables> writeVariables(const catalyst::utils::yaml::Configuration &config,
                                          catalyst::generate::buildwriters::BaseWriter &writer,
                                          const std::vector<FeatureFlag> &features,
                                          const catalyst::toolchain::ToolchainDef &tc) {

    catalyst::logger.debug("Writing variables to build file.");
    std::string build_dir_str = config.getBuildDir().string();

    auto build_sys_def =
        catalyst::toolchain::expandTemplate(tc.flags.define, {{"name", "CATALYST_BUILD_SYS"}, {"value", "1"}});
    auto proj_name_def = catalyst::toolchain::expandTemplate(
        tc.flags.define,
        {{"name", "CATALYST_PROJ_NAME"}, {"value", "\"" + config.getString("manifest.name").value_or("name") + "\""}});
    auto proj_ver_def = catalyst::toolchain::expandTemplate(
        tc.flags.define,
        {{"name", "CATALYST_PROJ_VER"},
         {"value", "\"" + config.getString("manifest.version").value_or("0.0.0") + "\""}});
    std::string default_defines = build_sys_def + " " + proj_name_def + " " + proj_ver_def;

    // Compiler and linker flags come from the toolchain. The manifest.tooling flag
    // overrides (CCFLAGS/CXXFLAGS/LDFLAGS) were removed in 1.7.0 in favour of
    // toolchain-defined flags (compiler.{c,cxx}.flags and linker.flags).
    std::string cxxflags = tc.compiler.cxx.flags + " " + default_defines;
    std::string ccflags = tc.compiler.c.flags + " " + default_defines;
    std::string ldflags =
        tc.linker.flags + " " + catalyst::toolchain::expandTemplate(tc.flags.lib_dir, {{"path", "catalyst-libs"}});

    auto definitions = featureDefinitions(config.getString("manifest.name").value_or("name"), features);

    std::vector<std::string> inc_dirs = config.getStringVector("manifest.dirs.include").value();
    for (const auto &inc_dir : inc_dirs) {
        cxxflags +=
            " " + catalyst::toolchain::expandTemplate(tc.flags.include_dir, {{"path", fs::absolute(inc_dir).string()}});
        ccflags +=
            " " + catalyst::toolchain::expandTemplate(tc.flags.include_dir, {{"path", fs::absolute(inc_dir).string()}});
    }

    std::string ldlibs;
    std::string rpaths;
    std::vector<std::string> rpath_flags;
    std::unordered_set<std::string> rpath_dirs;
    std::vector<std::string> link_inputs;
    if (ryml::ConstNodeRef deps = utils::yaml::child(config.rootRef(), "dependencies");
        deps.readable() && deps.is_seq()) {
        for (ryml::ConstNodeRef dep : deps.children()) {
            auto find_dep_res = findDep(build_dir_str, dep, tc);
            if (!find_dep_res) {
                return std::unexpected(find_dep_res.error());
            }
            const auto &dep_res = *find_dep_res;
            auto merged = mergeFeatureDefinitions(definitions, dep_res.definitions);
            if (!merged)
                return std::unexpected(merged.error());
            definitions = std::move(*merged);
            for (const auto &input : dep_res.link_inputs) {
                if (!std::ranges::contains(link_inputs, input))
                    link_inputs.push_back(input);
            }
            ldflags += " " + dep_res.lib_path;
            ldlibs += " " + dep_res.libs;
            ccflags += " " + dep_res.inc_path;
            cxxflags += " " + dep_res.inc_path;
            for (const auto &lib_dir : dep_res.lib_dirs) {
                if (!rpath_dirs.insert(lib_dir).second)
                    continue;
                std::string rpath = catalyst::toolchain::expandTemplate(tc.flags.rpath, {{"path", lib_dir}});
                if (rpath.empty())
                    continue;
                rpaths += (rpaths.empty() ? "" : " ") + rpath;
                rpath_flags.push_back(std::move(rpath));
            }
        }
    }

    for (const auto &[name, value] : definitions) {
        const auto flag =
            " " + catalyst::toolchain::expandTemplate(tc.flags.define, {{"name", name}, {"value", value}});
        cxxflags += flag;
        ccflags += flag;
    }

    writer.addComment("Variables");
    std::string cc_cmd = tc.compiler.c.executable;
    if (auto cc_launcher = config.getString("manifest.tooling.CC_LAUNCHER")) {
        cc_cmd = std::format("{} {}", cc_launcher.value(), cc_cmd);
    }
    void(writer.addVariable("cc", cc_cmd));

    std::string cxx_cmd = tc.compiler.cxx.executable;
    if (auto cxx_launcher = config.getString("manifest.tooling.CXX_LAUNCHER")) {
        cxx_cmd = std::format("{} {}", cxx_launcher.value(), cxx_cmd);
    }
    void(writer.addVariable("cxx", cxx_cmd));
    void(writer.addVariable("linker", tc.linker.executable.empty() ? cxx_cmd : tc.linker.executable));
    void(writer.addVariable("archiver", tc.archiver.executable));

    void(writer.addVariable("cxxflags", cxxflags));
    void(writer.addVariable("cflags", ccflags));
    void(writer.addVariable("ldflags", ldflags));
    void(writer.addVariable("rpaths", rpaths));
    void(writer.addVariable("ldlibs", ldlibs)); // place compiled libraries here
    return GeneratedVariables{
        .cxxflags = std::move(cxxflags), .rpaths = std::move(rpath_flags), .link_inputs = std::move(link_inputs)};
}

void writeRules(catalyst::generate::buildwriters::BaseWriter &writer, const catalyst::toolchain::ToolchainDef &tc) {
    catalyst::logger.debug("Writing rules to build file.");
    writer.addComment("Rules for compiling");
    // $extra_args carries per-edge flags (C++20 modules); it expands to
    // nothing on edges that don't set it.
    std::string cxx_compile = catalyst::toolchain::expandTemplate(tc.compiler.cxx.command,
                                                                  {{"cxx", "$cxx"},
                                                                   {"cxxflags", "$cxxflags $extra_args"},
                                                                   {"source", "$in"},
                                                                   {"object", "$out"},
                                                                   {"includes", ""},
                                                                   {"defines", ""}});
    std::string c_compile = catalyst::toolchain::expandTemplate(tc.compiler.c.command,
                                                                {{"cc", "$cc"},
                                                                 {"cflags", "$cflags"},
                                                                 {"source", "$in"},
                                                                 {"object", "$out"},
                                                                 {"includes", ""},
                                                                 {"defines", ""}});
    const bool cxx_has_depfile = cxx_compile.contains("$out.d");
    const bool c_has_depfile = c_compile.contains("$out.d");
    void(writer.addRule(
        "cxx_compile", cxx_compile, "CXX $out", cxx_has_depfile ? "$out.d" : "", cxx_has_depfile ? "gcc" : ""));
    void(writer.addRule("cc_compile", c_compile, "CC $out", c_has_depfile ? "$out.d" : "", c_has_depfile ? "gcc" : ""));

    writer.addComment("Rules for linking");
    void(writer.addRule("binary_link",
                        catalyst::toolchain::expandTemplate(tc.linker.executable_command,
                                                            {{"linker", "$linker"},
                                                             {"objects", "$in"},
                                                             {"output", "$out"},
                                                             {"ldflags", "$ldflags"},
                                                             {"lib_dirs", ""},
                                                             {"rpaths", "$rpaths"},
                                                             {"libs", "$ldlibs"}}),
                        "LINK $out"));
    void(writer.addRule("static_link",
                        catalyst::toolchain::expandTemplate(
                            tc.archiver.command, {{"archiver", "$archiver"}, {"objects", "$in"}, {"output", "$out"}}),
                        "LINK $out"));
    void(writer.addRule("shared_link",
                        catalyst::toolchain::expandTemplate(tc.linker.shared_lib_command,
                                                            {{"linker", "$linker"},
                                                             {"objects", "$in"},
                                                             {"output", "$out"},
                                                             {"ldflags", "$ldflags"},
                                                             {"lib_dirs", ""},
                                                             {"rpaths", "$rpaths"},
                                                             {"libs", "$ldlibs"}}),
                        "LINK $out"));
}

void featureFilter(std::unordered_set<fs::path> &source_set, const std::vector<FeatureFlag> &features) {
    std::unordered_set<fs::path> files_to_remove;
    for (const auto &ff : features) {
        if (ff.is_enabled)
            continue;

        for (const auto &file : ff.files) {
            files_to_remove.insert(fs::absolute(file));
        }
    }

    for (const auto &file : files_to_remove) {
        catalyst::logger.debug("Removing file: {} based on feature exclusion", file.string());
        source_set.erase(file);
    }
}
} // namespace

FeatureDefinitions featureDefinitions(std::string_view project, std::span<const FeatureFlag> features) {
    FeatureDefinitions definitions;
    for (const auto &feature : features) {
        const auto name = std::format("FF_{}__{}", project, feature.name);
        if (feature.type == "bool") {
            definitions[name] = feature.resolved_val == "true" ? "1" : "0";
        } else if (feature.type == "enum") {
            const auto active = std::ranges::find(feature.enum_values, feature.resolved_val);
            definitions[name] = std::to_string(std::distance(feature.enum_values.begin(), active));
            for (size_t i = 0; i < feature.enum_values.size(); ++i)
                definitions[name + "__" + feature.enum_values[i]] = std::to_string(i);
        } else if (feature.type == "string") {
            definitions[name] = "\"" + feature.resolved_val + "\"";
        } else {
            definitions[name] = feature.resolved_val;
        }
    }
    return definitions;
}

// The merge is symmetric: swapping its inputs cannot change the definitions or conflict outcome.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Result<FeatureDefinitions> mergeFeatureDefinitions(const FeatureDefinitions &left, const FeatureDefinitions &right) {
    auto merged = left;
    for (const auto &[name, value] : right) {
        const auto [entry, inserted] = merged.emplace(name, value);
        if (!inserted && entry->second != value)
            return std::unexpected(
                std::format("Conflicting feature definition '{}': '{}' versus '{}'", name, entry->second, value));
    }
    return merged;
}

Result<std::vector<FeatureFlag>> resolveFeatureFlags(const utils::yaml::Configuration &config,
                                                     const std::vector<std::string> &enabled_features) {
    namespace yaml = catalyst::utils::yaml;
    std::vector<FeatureFlag> resolved;

    // Parse CLI overrides first
    std::unordered_map<std::string, std::string> overrides;
    for (const auto &arg : enabled_features) {
        size_t eq_pos = arg.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = arg.substr(0, eq_pos);
            std::string value = arg.substr(eq_pos + 1);
            if (name.empty()) {
                return std::unexpected("Invalid CLI override format (empty feature name): '" + arg + "'");
            }
            overrides[name] = value;
        } else if (arg.starts_with("no-")) {
            std::string name = arg.substr(3);
            if (name.empty()) {
                return std::unexpected("Invalid CLI override format: '" + arg + "'");
            }
            overrides[name] = "false";
        } else {
            if (arg.empty()) {
                return std::unexpected("Invalid CLI override format: empty string");
            }
            overrides[arg] = "true";
        }
    }

    ryml::ConstNodeRef features_node = yaml::child(config.rootRef(), "features");
    if (!features_node.readable() || !features_node.is_map()) {
        return resolved;
    }

    for (ryml::ConstNodeRef feature_node : features_node.children()) {
        if (!feature_node.has_key())
            continue;

        FeatureFlag ff;
        ff.name = std::string{feature_node.key().str, feature_node.key().len};

        // Parse structure
        if (!feature_node.is_map()) {
            // Bare value -> boolean flag
            ff.type = "bool";
            bool val = yaml::asBool(feature_node).value_or(false);
            ff.default_val = val ? "true" : "false";
        } else {
            // Map
            if (yaml::child(feature_node, "type").readable()) {
                std::string t = yaml::asString(yaml::child(feature_node, "type")).value();
                if (t != "enum" && t != "int" && t != "string") {
                    return std::unexpected("Invalid feature flag type '" + t + "' for feature '" + ff.name + "'");
                }
                ff.type = t;

                // Validation: require default: for non-bool
                if (!yaml::child(feature_node, "default").readable()) {
                    return std::unexpected("Feature flag '" + ff.name + "' of type '" + t
                                           + "' requires a 'default' value");
                }

                if (t == "enum") {
                    ryml::ConstNodeRef vals_node = yaml::child(feature_node, "values");
                    if (!vals_node.readable() || !vals_node.is_seq()) {
                        return std::unexpected("Enum feature flag '" + ff.name + "' requires a list of 'values'");
                    }
                    for (ryml::ConstNodeRef val : vals_node.children()) {
                        ff.enum_values.push_back(yaml::asString(val).value_or(""));
                    }
                    if (ff.enum_values.empty()) {
                        return std::unexpected("Enum feature flag '" + ff.name + "' has empty 'values' list");
                    }
                    ff.default_val = yaml::asString(yaml::child(feature_node, "default")).value_or("");
                    if (std::ranges::find(ff.enum_values, ff.default_val) == ff.enum_values.end()) {
                        return std::unexpected("Default value '" + ff.default_val
                                               + "' is not in 'values' list for enum feature '" + ff.name + "'");
                    }
                } else if (t == "int") {
                    std::string def_str = yaml::asString(yaml::child(feature_node, "default")).value_or("");
                    try {
                        size_t pos = 0;
                        std::stoll(def_str, &pos);
                        if (pos != def_str.size()) {
                            return std::unexpected("Default value '" + def_str + "' for integer feature '" + ff.name
                                                   + "' is not a valid integer");
                        }
                    } catch (...) {
                        return std::unexpected("Default value '" + def_str + "' for integer feature '" + ff.name
                                               + "' is not a valid integer");
                    }
                    ff.default_val = def_str;
                } else if (t == "string") {
                    ff.default_val = yaml::asString(yaml::child(feature_node, "default")).value_or("");
                }
            } else {
                // Map but no type specified -> boolean flag
                ff.type = "bool";
                bool val = false;
                if (yaml::child(feature_node, "default").readable()) {
                    val = yaml::asBool(yaml::child(feature_node, "default")).value_or(false);
                }
                ff.default_val = val ? "true" : "false";
            }

            if (yaml::child(feature_node, "files").readable()) {
                if (auto files = yaml::asStringVector(yaml::child(feature_node, "files"))) {
                    ff.files = *files;
                }
            }
        }

        // Resolve value from CLI overrides
        auto it = overrides.find(ff.name);
        if (it != overrides.end()) {
            std::string cli_val = it->second;
            if (ff.type == "bool") {
                if (cli_val == "true" || cli_val == "1") {
                    ff.resolved_val = "true";
                } else if (cli_val == "false" || cli_val == "0") {
                    ff.resolved_val = "false";
                } else {
                    return std::unexpected("Invalid value '" + cli_val + "' for boolean feature '" + ff.name + "'");
                }
            } else {
                // Non-bool flag.
                // If they passed bare name (which parsed as "true") or no-prefix (which parsed as "false")
                // but this feature is non-bool:
                if (cli_val == "true") {
                    return std::unexpected("Feature '" + ff.name
                                           + "' is a non-boolean flag and cannot be enabled without a value (use "
                                           + ff.name + "=value).");
                }
                if (cli_val == "false") {
                    return std::unexpected("Feature '" + ff.name
                                           + "' is a non-boolean flag and cannot be disabled with 'no-' prefix.");
                }

                if (ff.type == "enum") {
                    if (std::find(ff.enum_values.begin(), ff.enum_values.end(), cli_val) == ff.enum_values.end()) {
                        return std::unexpected("Value '" + cli_val + "' is not a valid option for enum feature '"
                                               + ff.name + "'");
                    }
                    ff.resolved_val = cli_val;
                } else if (ff.type == "int") {
                    try {
                        size_t pos = 0;
                        std::stoll(cli_val, &pos);
                        if (pos != cli_val.size()) {
                            return std::unexpected("Value '" + cli_val + "' is not a valid integer for feature '"
                                                   + ff.name + "'");
                        }
                    } catch (...) {
                        return std::unexpected("Value '" + cli_val + "' is not a valid integer for feature '" + ff.name
                                               + "'");
                    }
                    ff.resolved_val = cli_val;
                } else if (ff.type == "string") {
                    ff.resolved_val = cli_val;
                }
            }
        } else {
            ff.resolved_val = ff.default_val;
        }

        // Set is_enabled
        if (ff.type == "bool") {
            ff.is_enabled = (ff.resolved_val == "true");
        } else {
            ff.is_enabled = (ff.resolved_val != "off" && ff.resolved_val != "false" && ff.resolved_val != "0"
                             && ff.resolved_val != "none" && !ff.resolved_val.empty());
        }

        resolved.push_back(std::move(ff));
    }

    return resolved;
}

} // namespace catalyst::generate
