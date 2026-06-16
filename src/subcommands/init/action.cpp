#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include "catalyst/globals.hpp"
#include "catalyst/subcommands/init.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::init {

namespace fs = std::filesystem;

Result<void> action(const Parse &parse_args) {
    catalyst::logger.debug("Init subcommand invoked.");
    namespace yaml = catalyst::utils::yaml;
    // TODO: change directory to parse_args->path;
    ryml::Tree tree;
    ryml::NodeRef root = tree.rootref();
    root |= ryml::MAP;

    ryml::NodeRef meta = yaml::childOrCreate(root, "meta");
    meta |= ryml::MAP;
    meta["min_ver"] = tree.to_arena(CATALYST_PROJ_VER);

    ryml::NodeRef manifest = yaml::childOrCreate(root, "manifest");
    manifest |= ryml::MAP;
    manifest["name"] = tree.to_arena(parse_args.name);
    switch (parse_args.type) {
        case Parse::Type::BINARY:
            manifest["type"] = "BINARY";
            break;
        case Parse::Type::STATICLIB:
            manifest["type"] = "STATICLIB";
            break;
        case Parse::Type::SHAREDLIB:
            manifest["type"] = "SHAREDLIB";
            break;
        case Parse::Type::INTERFACE:
            manifest["type"] = "INTERFACE";
            break;
    }
    manifest["version"] = tree.to_arena(parse_args.version);
    manifest["description"] = tree.to_arena(parse_args.description);
    manifest["provides"] = tree.to_arena(parse_args.provides);

    // Only emit tooling fields the user explicitly set. Anything omitted falls back
    // to the active toolchain's defaults, so we avoid pinning CC/CXX (and their flags)
    // into every generated profile.
    const auto &t = parse_args.tooling;
    if (!t.cc.empty() || !t.cxx.empty() || !t.ccflags.empty() || !t.cxxflags.empty() || !t.ldflags.empty()) {
        ryml::NodeRef tooling = yaml::childOrCreate(manifest, "tooling");
        tooling |= ryml::MAP;
        if (!t.cc.empty())
            tooling["CC"] = tree.to_arena(t.cc);
        if (!t.cxx.empty())
            tooling["CXX"] = tree.to_arena(t.cxx);
        if (!t.ccflags.empty())
            tooling["CCFLAGS"] = tree.to_arena(t.ccflags);
        if (!t.cxxflags.empty())
            tooling["CXXFLAGS"] = tree.to_arena(t.cxxflags);
        if (!t.ldflags.empty())
            tooling["LDFLAGS"] = tree.to_arena(t.ldflags);
    }

    ryml::NodeRef dirs = yaml::childOrCreate(manifest, "dirs");
    dirs |= ryml::MAP;
    auto add_dir_seq = [&tree, &dirs](std::string_view key, const std::vector<std::string> &values) {
        ryml::NodeRef seq = catalyst::utils::yaml::childOrCreate(dirs, key);
        seq |= ryml::SEQ;
        for (const auto &value : values)
            seq.append_child() = tree.to_arena(value);
    };
    add_dir_seq("include", parse_args.dirs.include);
    add_dir_seq("source", parse_args.dirs.source);
    dirs["build"] = tree.to_arena(parse_args.dirs.build);

    for (auto dir : parse_args.dirs.include) {
        if (!fs::exists(parse_args.path / dir)) {
            catalyst::logger.info("Creating include directory: {}", (parse_args.path / dir).string());
            fs::create_directories(parse_args.path / dir);
        }
    }

    for (auto dir : parse_args.dirs.source) {
        if (!fs::exists(parse_args.path / dir)) {
            catalyst::logger.info("Creating source directory: {}", (parse_args.path / dir).string());
            fs::create_directories(parse_args.path / dir);
        }
        auto ignore_path = fs::path(dir) / ".catalystignore";
        if (!fs::exists(ignore_path)) {
            catalyst::logger.info("Creating .catalystignore file in: {}", dir);
            std::ofstream{ignore_path};
        }
    }

    if (parse_args.type == Parse::Type::BINARY) {
        fs::path entry_cpp_path =
            parse_args.path / fs::path{parse_args.dirs.source[0]} / std::format("{}.cpp", parse_args.name);
        std::ofstream entry_cpp{entry_cpp_path};

        if (!entry_cpp) {
            return std::unexpected(std::format("Failed to create entry point while {} initializing BINARY project.",
                                               entry_cpp_path.string()));
        }
        entry_cpp <<
            R"(#include <iostream>

int main(int argc, char **argv) {
    std::cout << "Hello, Catalyst!" << std::endl;
}
)";
    }
    if (!fs::exists(parse_args.path / parse_args.dirs.build)) {
        catalyst::logger.info("Creating build directory: {}", (parse_args.path / parse_args.dirs.build).string());
        fs::create_directories(parse_args.path / parse_args.dirs.build);
    }

    fs::path profile_path;
    if (parse_args.profile == "common")
        profile_path = parse_args.path / std::format("catalyst.yaml");
    else
        profile_path = parse_args.path / std::format("catalyst_{}.yaml", parse_args.profile);

    if (!fs::exists(profile_path)) {
        catalyst::logger.debug("Creating new profile file: {}", profile_path.string());
    }

    std::ofstream profile_file = std::ofstream(profile_path);
    profile_file << yaml::emitYaml(tree) << std::flush;
    catalyst::logger.debug("Init subcommand finished successfully.");

    if (auto res = invokeIDEConfigEmitters(parse_args); !res) {
        return std::unexpected(res.error());
    }
    return {};
}

} // namespace catalyst::init
