#include <expected>
#include <filesystem>
#include <string>

#include "catalyst/subcommands/ide_sync.hpp"
#include "catalyst/subcommands/init.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

auto catalyst::ide_sync::action(const Parse &parse_args) -> Result<void> {
    catalyst::logger.debug("IDE sync subcommand invoked.");

    const std::filesystem::path root_dir = std::filesystem::current_path();

    const auto config = catalyst::utils::yaml::Configuration(parse_args.profiles);

    auto name = config.getString("manifest.name");
    if (!name)
        return std::unexpected("Invalid profile: missing manifest.name in common profile.");

    catalyst::init::Parse init_parse{
        .name = *name,
        .path = root_dir,
        .provides = "",
        .tooling = {},
        .dirs =
            {
                .include = config.getStringVector("manifest.dirs.include").value_or(std::vector<std::string>{}),
                .source = config.getStringVector("manifest.dirs.source").value_or(std::vector<std::string>{}),
                .build = config.getString("manifest.dirs.build").value_or(""),
            },
        .ides = parse_args.ides,
        .force_emit_ide = parse_args.force_emit_ide,
    };

    if (auto res = invokeIDEConfigEmitters(init_parse); !res)
        return std::unexpected(res.error());

    catalyst::logger.debug("IDE sync subcommand finished successfully.");
    return {};
}
