#include "catalyst/subcommands/add.hpp"

auto catalyst::add::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    return {app.add_subcommand("add", "Add a dependency."), std::make_unique<Parse>()};
}
