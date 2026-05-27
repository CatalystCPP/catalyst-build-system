#include <vector>

#include <CLI/App.hpp>

#include "catalyst/subcommands/build.hpp"

auto catalyst::build::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *build = app.add_subcommand("build", "Build the project.");
    auto ret = std::make_unique<Parse>();
    build->add_flag("-r,--regen", ret->regen, "Regenerate the build file.")->default_val(false);
    build->add_flag("-b,--force-rebuild", ret->force_rebuild, "Recompile dependencies.")->default_val(false);
    build->add_flag("--force-refetch", ret->force_refetch, "Refetch dependencies.")->default_val(false);
    build->add_flag("--workspace,--all", ret->workspace_build, "Build all members in the workspace.")
        ->default_val(false);
    build->add_flag("--watch,-w", ret->watch, "Continuous build mode. Rebuilds on source file changes.")
        ->default_val(false);
    build->add_option("-P,--package", ret->package, "Build a specific package from the root.");
    build->add_option("-p,--profiles", ret->profiles, "Profile composition to build.")
        ->default_val(std::vector{"common"});
    build->add_option("-f,--features", ret->enabled_features, "Features to enable.")
        ->default_val(std::vector<std::string>{});
    build->add_option("--backend", ret->backend, "Backend to use for generation (ninja, gmake, cbe).");
    return {build, std::move(ret)};
}
