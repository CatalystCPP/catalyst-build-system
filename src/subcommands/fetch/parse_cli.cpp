#include <CLI/App.hpp>

#include "catalyst/subcommands/fetch.hpp"

namespace catalyst::fetch {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *fetch = app.add_subcommand("fetch", "Fetch all dependencies for a profile composition.");
    auto ret = std::make_unique<Parse>();
    fetch->add_option("-p,--profiles", ret->profiles);
    return {fetch, std::move(ret)};
}
}; // namespace catalyst::fetch
