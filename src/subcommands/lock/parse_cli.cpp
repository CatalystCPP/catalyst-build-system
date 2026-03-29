#include <vector>

#include <CLI/App.hpp>

#include "catalyst/subcommands/lock.hpp"

namespace catalyst::lock {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *lock = app.add_subcommand("lock", "Resolve and pin dependencies to a catalyst.lock file.");
    auto ret = std::make_unique<Parse>();
    lock->add_option("-p,--profiles", ret->profiles, "Profile composition to use for dependency resolution.")
        ->default_val(std::vector<std::string>{"common"});
    return {lock, std::move(ret)};
}
} // namespace catalyst::lock
