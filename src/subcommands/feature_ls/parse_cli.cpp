#include <CLI/App.hpp>

#include "catalyst/subcommands/feature_ls.hpp"

namespace catalyst::feature_ls {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *feature_ls = app.add_subcommand("feature-ls", "list all features across all profiles");
    auto ret = std::make_unique<Parse>();
    app.add_flag("--inverse", ret->inverse, "list no-features as well");
    return {feature_ls, std::move(ret)};
}
} // namespace catalyst::feature_ls
