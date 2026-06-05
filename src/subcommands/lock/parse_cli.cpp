#include <vector>

#include <CLI11.hpp>

#include "catalyst/subcommands/lock.hpp"

auto catalyst::lock::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *lock = app.add_subcommand("lock", "Resolve and pin dependencies to a catalyst.lock file.");
    auto ret = std::make_unique<Parse>();
    lock->add_option("-p,--profiles", ret->profiles, "Profile composition to use for dependency resolution.")
        ->default_val(std::vector<std::string>{"common"});
    return {lock, std::move(ret)};
}
