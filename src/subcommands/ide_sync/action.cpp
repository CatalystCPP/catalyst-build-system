#include <expected>
#include <filesystem>
#include <string>

#include <yaml-cpp/node/node.h>

#include "catalyst/subcommands/ide_sync.hpp"
#include "catalyst/subcommands/init.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

auto catalyst::ide_sync::action(const Parse &parse_args) -> std::expected<void, std::string> {
    catalyst::logger.debug("IDE sync subcommand invoked.");

    const std::filesystem::path root_dir = std::filesystem::current_path();

    const auto config = catalyst::utils::yaml::Configuration(parse_args.profiles);
    const auto &profile_node = config.getRoot();

    if (!profile_node["manifest"] || !profile_node["manifest"]["name"])
        return std::unexpected("Invalid profile: missing manifest.name in common profile.");

    catalyst::init::Parse init_parse{
        .name = profile_node["manifest"]["name"].as<std::string>(),
        .path = root_dir,
        .provides = "",
        .tooling = {},
        .dirs =
            {
                .include = (profile_node["manifest"]["dirs"] && profile_node["manifest"]["dirs"]["include"])
                               ? profile_node["manifest"]["dirs"]["include"].as<std::vector<std::string>>()
                               : std::vector<std::string>{},
                .source = (profile_node["manifest"]["dirs"] && profile_node["manifest"]["dirs"]["source"])
                              ? profile_node["manifest"]["dirs"]["source"].as<std::vector<std::string>>()
                              : std::vector<std::string>{},
                .build = (profile_node["manifest"]["dirs"] && profile_node["manifest"]["dirs"]["build"])
                             ? profile_node["manifest"]["dirs"]["build"].as<std::string>()
                             : std::string{},
            },
        .ides = parse_args.ides,
        .force_emit_ide = parse_args.force_emit_ide,
    };

    if (auto res = invokeIDEConfigEmitters(init_parse); !res)
        return std::unexpected(res.error());

    catalyst::logger.debug("IDE sync subcommand finished successfully.");
    return {};
}
