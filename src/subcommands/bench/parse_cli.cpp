#include <memory>

#include <CLI11.hpp>

#include "catalyst/subcommands/bench.hpp"

auto catalyst::bench::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *bench = app.add_subcommand("bench", "Execute all benchmarks of a local package.");
    auto ret = std::make_unique<Parse>();
    bench->add_option("-P,--params", ret->params, "Params to pass to the bench executable.");
    bench->add_flag("-r,--rebuild", ret->rebuild, "Rebuild before benchmarking.")->default_val(false);
    return {bench, std::move(ret)};
}
