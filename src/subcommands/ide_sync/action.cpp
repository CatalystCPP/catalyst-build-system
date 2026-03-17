#include <expected>
#include <filesystem>
#include <string>

#include <yaml-cpp/node/node.h>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/subcommands/ide_sync.hpp"
#include "catalyst/subcommands/init.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::ide_sync {

namespace fs = std::filesystem;

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.debug("IDE sync subcommand invoked.");

    const fs::path root_dir = fs::current_path();

    const auto config = catalyst::utils::yaml::Configuration(parse_args.profiles);
    const auto &profile_node = config.getRoot();

    if (!profile_node["manifest"] || !profile_node["manifest"]["name"]) {
        return std::unexpected("Invalid profile: missing manifest.name in common profile.");
    }

    catalyst::init::Parse init_parse;
    init_parse.name = profile_node["manifest"]["name"].as<std::string>();
    init_parse.path = root_dir;
    init_parse.force_emit_ide = parse_args.force_emit_ide;

    if (profile_node["manifest"]["dirs"] && profile_node["manifest"]["dirs"]["include"]) {
        init_parse.dirs.include = profile_node["manifest"]["dirs"]["include"].as<std::vector<std::string>>();
    }
    if (profile_node["manifest"]["dirs"] && profile_node["manifest"]["dirs"]["source"]) {
        init_parse.dirs.source = profile_node["manifest"]["dirs"]["source"].as<std::vector<std::string>>();
    }
    if (profile_node["manifest"]["dirs"] && profile_node["manifest"]["dirs"]["build"]) {
        init_parse.dirs.build = profile_node["manifest"]["dirs"]["build"].as<std::string>();
    }

    if (auto res = invokeIDEConfigEmitters(init_parse); !res)
        return std::unexpected(res.error());

    catalyst::logger.debug("IDE sync subcommand finished successfully.");
    return {};
}

} // namespace catalyst::ide_sync
