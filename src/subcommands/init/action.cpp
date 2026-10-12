#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>

#include "catalyst/globals.hpp"
#include "catalyst/subcommands/init.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/toolchain.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::init {

namespace fs = std::filesystem;

namespace {
/// Returns true (and warns) when `path` already exists, so init never clobbers a user's file.
bool skipExisting(const fs::path &path) {
    if (!fs::exists(path))
        return false;
    catalyst::logger.warn("{} already exists; leaving it untouched.", path.string());
    return true;
}

fs::path profilePath(const Parse &parse_args) {
    if (parse_args.profile == "common")
        return parse_args.path / "catalyst.yaml";
    return parse_args.path / std::format("catalyst_{}.yaml", parse_args.profile);
}

/// A non-common profile is composed on top of common, and sequences accumulate during composition, so it starts out
/// as an empty overlay rather than a copy of the common manifest.
Result<void> writeOverlayProfile(const Parse &parse_args, const fs::path &profile_path) {
    std::ofstream profile_file{profile_path};
    profile_file << std::format("# Overlay for the '{}' profile, composed on top of catalyst.yaml (common).\n"
                                "# Only list the fields this profile changes.\n"
                                "{{}}\n",
                                parse_args.profile)
                 << std::flush;
    if (!profile_file)
        return std::unexpected(std::format("Failed to write profile file {}.", profile_path.string()));
    return {};
}
} // namespace

Result<void> action(const Parse &parse_args) {
    catalyst::logger.debug("Init subcommand invoked.");
    namespace yaml = catalyst::utils::yaml;

    const fs::path profile_path = profilePath(parse_args);
    if (parse_args.profile != "common") {
        if (!skipExisting(profile_path)) {
            catalyst::logger.debug("Creating new profile file: {}", profile_path.string());
            if (auto res = writeOverlayProfile(parse_args, profile_path); !res)
                return res;
        }
        return invokeIDEConfigEmitters(parse_args);
    }

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
    manifest["toolchain"] = tree.to_arena("tc_catalyst.yaml");

    // Compiler executables and compiler/linker flags now live in the toolchain file,
    // so init no longer emits any manifest.tooling overrides.

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

    for (const auto &dir : parse_args.dirs.include) {
        if (!fs::exists(parse_args.path / dir)) {
            catalyst::logger.info("Creating include directory: {}", (parse_args.path / dir).string());
            fs::create_directories(parse_args.path / dir);
        }
    }

    for (const auto &dir : parse_args.dirs.source) {
        if (!fs::exists(parse_args.path / dir)) {
            catalyst::logger.info("Creating source directory: {}", (parse_args.path / dir).string());
            fs::create_directories(parse_args.path / dir);
        }
        auto ignore_path = parse_args.path / dir / ".catalystignore";
        if (!fs::exists(ignore_path)) {
            catalyst::logger.info("Creating .catalystignore file in: {}", (parse_args.path / dir).string());
            std::ofstream{ignore_path};
        }
    }

    if (fs::path entry_cpp_path =
            parse_args.path / fs::path{parse_args.dirs.source[0]} / std::format("{}.cpp", parse_args.name);
        parse_args.type == Parse::Type::BINARY && !skipExisting(entry_cpp_path)) {
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

    if (!skipExisting(profile_path)) {
        catalyst::logger.debug("Creating new profile file: {}", profile_path.string());
        std::ofstream profile_file{profile_path};
        profile_file << yaml::emitYaml(tree) << std::flush;
        if (!profile_file)
            return std::unexpected(std::format("Failed to write profile file {}.", profile_path.string()));
    }

    if (const fs::path toolchain_path = parse_args.path / "tc_catalyst.yaml"; !skipExisting(toolchain_path)) {
        catalyst::logger.debug("Creating toolchain file: {}", toolchain_path.string());
        auto defaults = catalyst::toolchain::resolveToolchain(std::nullopt);
        if (!defaults)
            return std::unexpected(defaults.error());
        defaults->name = "Catalyst Default Toolchain";
        defaults->extensions.shell_scripts = {".sh"};
        std::ofstream toolchain_file{toolchain_path};
        toolchain_file << "# Catalyst Default Toolchain; feel free to modify this file to suit your needs.\n"
                       << catalyst::toolchain::serializeToolchain(*defaults);
        if (!toolchain_file)
            return std::unexpected("Failed to write default toolchain file.");
    }

    if (auto res = invokeIDEConfigEmitters(parse_args); !res) {
        return std::unexpected(res.error());
    }
    catalyst::logger.debug("Init subcommand finished successfully.");
    return {};
}

} // namespace catalyst::init
