#include "catalyst/subcommands/run.hpp"

auto catalyst::run::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *run = app.add_subcommand("run", "Run a built executable.");
    auto ret = std::make_unique<Parse>();
    run->add_option("-p,--profile", ret->profile)->default_val("common");
    run->add_option("-P,--params", ret->params)->default_val(std::vector<std::string>{});
    return {run, std::move(ret)};
}
