#include "catalyst/subcommands/profile_ls.hpp"

auto catalyst::profile_ls::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    auto *subcommand = app.add_subcommand("profile-ls", "list all profiles");
    auto result = std::make_unique<Parse>();
    subcommand->add_flag("--json", result->json, "Output a sorted JSON array for scripts and editor integrations.");
    return {subcommand, std::move(result)};
}
