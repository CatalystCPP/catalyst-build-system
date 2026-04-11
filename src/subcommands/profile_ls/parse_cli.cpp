#include "catalyst/subcommands/profile_ls.hpp"

auto catalyst::profile_ls::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    return {app.add_subcommand("profile-ls", "list all profiles"), std::make_unique<Parse>()};
}
