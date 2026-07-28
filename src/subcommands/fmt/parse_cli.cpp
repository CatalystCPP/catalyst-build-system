#include "catalyst/subcommands/fmt.hpp"

auto catalyst::fmt::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *fmt = app.add_subcommand("fmt", "Format project source files.");
    auto ret = std::make_unique<Parse>();
    fmt->add_option("-p,--profiles", ret->profiles, "Profile composition to build.")
        ->default_val(std::vector{"common"});
    return {fmt, std::move(ret)};
}
