#include "catalyst/subcommands/fmt.hpp"

auto catalyst::fmt::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    return {app.add_subcommand("fmt", "Format project source files."), std::make_unique<Parse>()};
}
