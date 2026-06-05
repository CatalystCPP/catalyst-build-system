#include <vector>

#include <CLI11.hpp>

#include "catalyst/subcommands/tidy.hpp"

auto catalyst::tidy::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *tidy = app.add_subcommand("tidy", "Run linting on source code.");
    auto ret = std::make_unique<Parse>();
    tidy->add_option("-p,--profiles", ret->profiles, "The profile composition to lint")
        ->default_val(std::vector<std::string>{"common"});
    tidy->add_flag("--fix", ret->fix, "Automatically fix lint warnings");
    return {tidy, std::move(ret)};
}
