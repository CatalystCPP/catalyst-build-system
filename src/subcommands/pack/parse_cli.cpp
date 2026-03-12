#include <memory>
#include <CLI/App.hpp>

#include "catalyst/subcommands/pack.hpp"

namespace catalyst::pack {

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *pack = app.add_subcommand("pack", "Assemble the local package for distribution.");
    auto ret = std::make_unique<Parse>();
    return {pack, std::move(ret)};
}
} // namespace catalyst::pack
