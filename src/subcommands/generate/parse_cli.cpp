#include "catalyst/subcommands/generate.hpp"

auto catalyst::generate::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *generate = app.add_subcommand("generate", "Generate a build file.");
    auto ret = std::make_unique<Parse>();
    generate->add_option("-p,--profiles", ret->profiles);
    generate->add_option("-f,--features", ret->enabled_features);
    generate->add_option("-b,--backend", ret->backend, "Backend to use for generation (ninja, gmake, cob).");
    return {generate, std::move(ret)};
}
