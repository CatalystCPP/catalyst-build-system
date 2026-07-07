#pragma once
#include <expected>
#include <utility>

#include <CLI11.hpp>

#include "catalyst/subcommands/init.hpp"
#include "catalyst/utils/result.hpp"

namespace catalyst::ide_sync {
struct Parse {
    using IdeType = catalyst::init::Parse::IdeType;

    std::vector<std::string> profiles; ///< profiles to sync
    std::vector<IdeType> ides;         ///< IDEs to sync
    bool force_emit_ide;               ///< Override existing IDE files
};
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::ide_sync
