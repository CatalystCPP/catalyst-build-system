#include <sys/wait.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
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

namespace {

bool isEnabled(bool default_enabled,
               const std::string &feature,
               const std::unordered_set<std::string> &enabled_features);

// A feature's default state: either `feature: <bool>` or `feature: {default: <bool>}`.
bool featureDefault(ryml::ConstNodeRef feature_node);

void writeVariables(const catalyst::utils::yaml::Configuration &config,
                    catalyst::generate::buildwriters::BaseWriter &writer,
                    const std::vector<std::string> &enabled_features,
                    const catalyst::toolchain::ToolchainDef &tc);
void writeRules(catalyst::generate::buildwriters::BaseWriter &writer, const catalyst::toolchain::ToolchainDef &tc);
std::vector<std::string> intermediateTargets(catalyst::generate::buildwriters::BaseWriter &writer,
                                             const std::unordered_set<fs::path> &source_set,
                                             const catalyst::toolchain::ToolchainDef &tc);
void finalTarget(const utils::yaml::Configuration &config,
                 const auto &object_files,
                 catalyst::generate::buildwriters::BaseWriter &writer,
                 const catalyst::toolchain::ToolchainDef &tc);

void featureFilter(std::unordered_set<fs::path> &source_set,
                   const catalyst::utils::yaml::Configuration &config,
                   const std::vector<std::string> &enabled_features);

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

    catalyst::logger.debug("Running pre-generate hooks.");
    if (auto res = hooks::preGenerate(config); !res) {
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
    featureFilter(source_set, config, parse_args.enabled_features);

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

    std::string build_filename;
    if (generator == "ninja") {
        build_filename = "build.ninja";
    } else if (generator == "gmake" || generator == "make") {
        build_filename = "Makefile";
    } else {
        build_filename = "catalyst.build";
    }

    const fs::path buildfile_path = build_dir / build_filename;
    catalyst::logger.debug("Writing build file to: {}", buildfile_path.string());
    std::ofstream buildfile{buildfile_path};

    if (!buildfile) {
        return std::unexpected(std::format("Failed to open {} for writing", buildfile_path.string()));
    }

    catalyst::toolchain::ToolchainDef tc;
    if (auto toolchain_path = config.getString("manifest.toolchain")) {
        auto parsed = catalyst::toolchain::parse_toolchain(*toolchain_path);
        if (!parsed) {
            return std::unexpected(std::format("Failed to load toolchain {}: {}", *toolchain_path, parsed.error()));
        }
        tc = std::move(*parsed);
    }

    auto generate_build = [&](buildwriters::BaseWriter &writer) {
        writer.addComment("Build file generated by Catalyst");
        writeVariables(config, writer, parse_args.enabled_features, tc);
        writeRules(writer, tc);
        std::vector<std::string> object_files = intermediateTargets(writer, source_set, tc);
        finalTarget(config, object_files, writer, tc);
    };

    if (generator == "ninja") {
        buildwriters::DerivedWriter<buildwriters::TargetType::Ninja> writer(buildfile);
        generate_build(writer);
    } else if (generator == "gmake" || generator == "make") {
        buildwriters::DerivedWriter<buildwriters::TargetType::Make> writer(buildfile);
        generate_build(writer);
    } else {
        buildwriters::DerivedWriter<buildwriters::TargetType::COB> writer(buildfile);
        generate_build(writer);
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

std::vector<std::string> intermediateTargets(catalyst::generate::buildwriters::BaseWriter &writer,
                                             const std::unordered_set<std::filesystem::path> &source_set,
                                             const catalyst::toolchain::ToolchainDef &tc) {
    catalyst::logger.debug("Generate subcommand invoked.");
    fs::path current_dir = fs::current_path();
    writer.addComment("Source File Compilation");
    std::vector<std::string> object_files;
    for (const auto &src : source_set) {
        fs::path relative_src_path = fs::relative(src, current_dir);
        std::string obj_name = std::format("{}_{:016x}{}",
                                           relative_src_path.stem().string(),
                                           fnv1aHash(relative_src_path.string()),
                                           tc.extensions.object);
        object_files.push_back((fs::path{"obj"} / obj_name).string());
        void(writer.addBuild({object_files.back()},
                             ((src.extension() == ".c" || src.extension() == ".cu") ? "cc_compile" : "cxx_compile"),
                             {src.string()}));
    }
    return object_files;
}

void finalTarget(const utils::yaml::Configuration &config,
                 const auto &object_files,
                 catalyst::generate::buildwriters::BaseWriter &writer,
                 const catalyst::toolchain::ToolchainDef &tc) {
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
    void(writer.addBuild({target_path.string()}, link_rule, object_files));

    // Default target
    writer.addComment("Default target to build");
    writer.addDefault(target_path.string());
}

void writeVariables(const catalyst::utils::yaml::Configuration &config,
                    catalyst::generate::buildwriters::BaseWriter &writer,
                    const std::vector<std::string> &enabled_features,
                    const catalyst::toolchain::ToolchainDef &tc) {

    catalyst::logger.debug("Writing variables to build file.");
    std::string build_dir_str = config.getBuildDir().string();

    auto build_sys_def =
        catalyst::toolchain::expand_template(tc.flags.define, {{"name", "CATALYST_BUILD_SYS"}, {"value", "1"}});
    auto proj_name_def = catalyst::toolchain::expand_template(
        tc.flags.define,
        {{"name", "CATALYST_PROJ_NAME"}, {"value", "\"" + config.getString("manifest.name").value_or("name") + "\""}});
    auto proj_ver_def = catalyst::toolchain::expand_template(
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
        tc.linker.flags + " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", "catalyst-libs"}});

    if (const char *vcpkg_root = std::getenv("VCPKG_ROOT"); vcpkg_root != nullptr) {
#if defined(_WIN32)
        const char *triplet = "x64-windows";
#elif defined(__APPLE__)
        const char *triplet = "x64-osx";
#else
        const char *triplet = "x64-linux";
#endif
        cxxflags += " " + catalyst::toolchain::expand_template(
                              tc.flags.include_dir,
                              {{"path", (fs::path(vcpkg_root) / "installed" / triplet / "include").string()}});
        ccflags += " " + catalyst::toolchain::expand_template(
                             tc.flags.include_dir,
                             {{"path", (fs::path(vcpkg_root) / "installed" / triplet / "include").string()}});
        ldflags +=
            " " + catalyst::toolchain::expand_template(
                      tc.flags.lib_dir, {{"path", (fs::path(vcpkg_root) / "installed" / triplet / "lib").string()}});
    } else {
        logger.warn("VCPKG_ROOT environment variable is not defined.");
    }

    std::unordered_set<std::string> enabled_features_set(enabled_features.begin(), enabled_features.end());
    if (ryml::ConstNodeRef features_node = utils::yaml::child(config.rootRef(), "features");
        features_node.readable() && features_node.is_map()) {
        for (ryml::ConstNodeRef feature_node : features_node.children()) {
            if (!feature_node.has_key())
                continue;
            std::string feature{feature_node.key().str, feature_node.key().len};
            bool default_enabled = featureDefault(feature_node);

            std::string flag =
                " " +
                catalyst::toolchain::expand_template(
                    tc.flags.define,
                    {{"name", std::format("FF_{}__{}", config.getString("manifest.name").value_or("name"), feature)},
                     {"value", isEnabled(default_enabled, feature, enabled_features_set) ? "1" : "0"}});
            cxxflags += flag;
            ccflags += flag;
        }
    }

    std::vector<std::string> inc_dirs = config.getStringVector("manifest.dirs.include").value();
    for (const auto &inc_dir : inc_dirs) {
        cxxflags += " " + catalyst::toolchain::expand_template(tc.flags.include_dir,
                                                               {{"path", fs::absolute(inc_dir).string()}});
        ccflags += " " + catalyst::toolchain::expand_template(tc.flags.include_dir,
                                                              {{"path", fs::absolute(inc_dir).string()}});
    }

    std::string ldlibs;
    if (ryml::ConstNodeRef deps = utils::yaml::child(config.rootRef(), "dependencies");
        deps.readable() && deps.is_seq()) {
        for (ryml::ConstNodeRef dep : deps.children()) {
            auto find_dep_res = findDep(build_dir_str, dep, tc);
            if (!find_dep_res) {
                catalyst::logger.warn("Partial failure in dependency resolution: {}", find_dep_res.error());
                continue;
            }
            const auto &dep_res = *find_dep_res;
            ldflags += " " + dep_res.lib_path;
            ldlibs += " " + dep_res.libs;
            ccflags += " " + dep_res.inc_path;
            cxxflags += " " + dep_res.inc_path;
        }
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
    void(writer.addVariable("ldlibs", ldlibs)); // place compiled libraries here
}

void writeRules(catalyst::generate::buildwriters::BaseWriter &writer, const catalyst::toolchain::ToolchainDef &tc) {
    catalyst::logger.debug("Writing rules to build file.");
    writer.addComment("Rules for compiling");
    std::string cxx_compile = catalyst::toolchain::expand_template(tc.compiler.cxx.command,
                                                                   {{"cxx", "$cxx"},
                                                                    {"cxxflags", "$cxxflags"},
                                                                    {"source", "$in"},
                                                                    {"object", "$out"},
                                                                    {"includes", ""},
                                                                    {"defines", ""}});
    std::string c_compile = catalyst::toolchain::expand_template(tc.compiler.c.command,
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
                        catalyst::toolchain::expand_template(tc.linker.executable_command,
                                                             {{"linker", "$linker"},
                                                              {"objects", "$in"},
                                                              {"output", "$out"},
                                                              {"ldflags", "$ldflags"},
                                                              {"lib_dirs", ""},
                                                              {"libs", "$ldlibs"}}),
                        "LINK $out"));
    void(writer.addRule("static_link",
                        catalyst::toolchain::expand_template(
                            tc.archiver.command, {{"archiver", "$archiver"}, {"objects", "$in"}, {"output", "$out"}}),
                        "LINK $out"));
    void(writer.addRule("shared_link",
                        catalyst::toolchain::expand_template(tc.linker.shared_lib_command,
                                                             {{"linker", "$linker"},
                                                              {"objects", "$in"},
                                                              {"output", "$out"},
                                                              {"ldflags", "$ldflags"},
                                                              {"lib_dirs", ""},
                                                              {"libs", "$ldlibs"}}),
                        "LINK $out"));
}

void featureFilter(std::unordered_set<fs::path> &source_set,
                   const catalyst::utils::yaml::Configuration &config,
                   const std::vector<std::string> &enabled_features) {
    namespace yaml = catalyst::utils::yaml;
    std::unordered_set<std::string> enabled_features_set(enabled_features.begin(), enabled_features.end());

    ryml::ConstNodeRef features_node = yaml::child(config.rootRef(), "features");
    if (!features_node.readable() || !features_node.is_map())
        return;

    std::unordered_set<fs::path> files_to_remove;
    for (ryml::ConstNodeRef feature_node : features_node.children()) {
        if (!feature_node.has_key())
            continue;
        std::string feature{feature_node.key().str, feature_node.key().len};
        if (isEnabled(featureDefault(feature_node), feature, enabled_features_set))
            continue;

        if (auto excluded_files = yaml::asStringVector(yaml::child(feature_node, "files"))) {
            for (const auto &file : *excluded_files) {
                files_to_remove.insert(fs::absolute(file));
            }
        }
    }

    for (const auto &file : files_to_remove) {
        catalyst::logger.debug("Removing file: {} based on feature exclusion", file.string());
        source_set.erase(file);
    }
}

bool featureDefault(ryml::ConstNodeRef feature_node) {
    namespace yaml = catalyst::utils::yaml;
    if (feature_node.is_map()) {
        return yaml::asBool(yaml::child(feature_node, "default")).value_or(false);
    }
    return yaml::asBool(feature_node).value_or(false);
}

bool isEnabled(bool default_enabled,
               const std::string &feature,
               const std::unordered_set<std::string> &enabled_features) {
    bool explicitly_enabled = enabled_features.contains(feature);
    bool explicitly_disabled = enabled_features.contains("no-" + feature);

    bool is_enabled = default_enabled;
    if (explicitly_enabled) {
        is_enabled = true;
    } else if (explicitly_disabled) {
        is_enabled = false;
    }
    return is_enabled;
}
} // namespace

} // namespace catalyst::generate
