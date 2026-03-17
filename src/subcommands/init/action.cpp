#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include <yaml-cpp/node/node.h>

#include "catalyst/globals.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/subcommands/init.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"

namespace catalyst::init {

namespace fs = std::filesystem;

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.debug("Init subcommand invoked.");
    // TODO: change directory to parse_args->path;
    YAML::Node node;
    node["meta"]["min_ver"] = catalyst::CATALYST_VERSION;
    node["manifest"]["name"] = parse_args.name;
    switch (parse_args.type) {
        case Parse::Type::BINARY:
            node["manifest"]["type"] = "BINARY";
            break;
        case Parse::Type::STATICLIB:
            node["manifest"]["type"] = "STATICLIB";
            break;
        case Parse::Type::SHAREDLIB:
            node["manifest"]["type"] = "SHAREDLIB";
            break;
        case Parse::Type::INTERFACE:
            node["manifest"]["type"] = "INTERFACE";
            break;
    }
    node["manifest"]["version"] = parse_args.version;
    node["manifest"]["description"] = parse_args.description;
    node["manifest"]["provides"] = parse_args.provides;
    node["manifest"]["tooling"]["CC"] = parse_args.tooling.cc;
    node["manifest"]["tooling"]["CXX"] = parse_args.tooling.cxx;
    node["manifest"]["tooling"]["CCFLAGS"] = parse_args.tooling.ccflags;
    node["manifest"]["tooling"]["CXXFLAGS"] = parse_args.tooling.cxxflags;
    node["manifest"]["tooling"]["LDFLAGS"] = parse_args.tooling.ldflags;
    node["manifest"]["dirs"]["include"] = parse_args.dirs.include;
    node["manifest"]["dirs"]["source"] = parse_args.dirs.source;
    node["manifest"]["dirs"]["build"] = parse_args.dirs.build;

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
    YAML::Emitter emmiter;
    emmiter << node;
    profile_file << emmiter.c_str() << std::endl;
    catalyst::logger.debug("Init subcommand finished successfully.");

    if (auto res = invokeIDEConfigEmitters(parse_args); !res) {
        return std::unexpected(res.error());
    }
    return {};
}

} // namespace catalyst::init
