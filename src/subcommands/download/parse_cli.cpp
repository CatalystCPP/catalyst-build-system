#include <memory>

#include "catalyst/subcommands/download.hpp"

auto catalyst::download::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    auto download = app.add_subcommand("download", "Install a catalyst configured package from git.");
    auto ret = std::make_unique<Parse>();
    download->add_option("remote", ret->git_remote, "the remote to clone")->required();
    download->add_option("branch", ret->git_branch, "the branch to clone");
    download->add_option("-p,--profiles", ret->profiles, "the profiles to compose in the build artifact")
        ->default_val(std::vector<std::string>{"common"});
    download->add_option("-f,--features", ret->enabled_features, "the features to enable in the build");
    download->add_option("-b,--backend", ret->backend, "Backend to use for generation (ninja, gmake, cob).");
    download->add_option("-t,--target", ret->target_path, "the path to install to")->required();
    return {download, std::move(ret)};
}
