#include <CLI/App.hpp>

#include "catalyst/subcommands/profile_ls.hpp"

namespace catalyst::profile_ls {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *profile_ls = app.add_subcommand("profile-ls", "list all profiles");
    auto ret = std::make_unique<Parse>();
    return {profile_ls, std::move(ret)};
}
} // namespace catalyst::profile_ls
