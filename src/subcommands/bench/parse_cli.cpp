#include <memory>
#include <CLI/App.hpp>

#include "catalyst/subcommands/bench.hpp"

namespace catalyst::bench {

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *bench = app.add_subcommand("bench", "Execute all benchmarks of a local package.");
    auto ret = std::make_unique<Parse>();
    return {bench, std::move(ret)};
}
} // namespace catalyst::bench
