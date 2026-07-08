#include "catalyst/subcommands/generate.hpp"

auto catalyst::generate::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *generate = app.add_subcommand("generate", "Generate a build file.");
    auto ret = std::make_unique<Parse>();
    generate->add_option("-p,--profiles", ret->profiles, "Profile composition to generate.")
        ->default_val(std::vector<std::string>{"common"});
    generate->add_option("-f,--features", ret->enabled_features, "Features to enable.");
    generate->add_option("-b,--backend", ret->backend, "Backend to use for generation (ninja, gmake, cob).");
    return {generate, std::move(ret)};
}
