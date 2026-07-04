#include <CLI11.hpp>

#include "catalyst/subcommands/fetch.hpp"

auto catalyst::fetch::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *fetch = app.add_subcommand("fetch", "Fetch all dependencies for a profile composition.");
    auto ret = std::make_unique<Parse>();
    fetch->add_option("-p,--profiles", ret->profiles, "Profile composition to fetch.")
        ->default_val(std::vector<std::string>{"common"});
    return {fetch, std::move(ret)};
}
