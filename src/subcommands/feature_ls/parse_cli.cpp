#include <CLI11.hpp>

#include "catalyst/subcommands/feature_ls.hpp"

auto catalyst::feature_ls::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *feature_ls = app.add_subcommand("feature-ls", "list all features across all profiles");
    auto ret = std::make_unique<Parse>();
    feature_ls->add_flag("--inverse", ret->inverse, "list no-features as well");
    return {feature_ls, std::move(ret)};
}
