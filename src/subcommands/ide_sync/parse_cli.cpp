#include <map>
#include <string>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/subcommands/ide_sync.hpp"

auto catalyst::ide_sync::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *ide_sync = app.add_subcommand("ide-sync", "Sync IDE configuration files for an existing project.");
    auto ret = std::make_unique<Parse>();

    const std::map<std::string, Parse::IdeType> ide_map{
        {"vscode", Parse::IdeType::vsc},
        {"clion", Parse::IdeType::clion},
    };

    ide_sync->add_option("-p,--profiles", ret->profiles, "Profiles to use from the configuration file")
        ->default_val("common");
    ide_sync->add_option("--ides", ret->ides, "IDEs to generate project files for: vscode, clion")
        ->transform(CLI::CheckedTransformer(ide_map, CLI::ignore_case).description(""));
    ide_sync->add_flag("-f,--force-ide", ret->force_emit_ide, "force emitting IDE config even if one already exists")
        ->default_val(false);

    return {ide_sync, std::move(ret)};
}
