#pragma once
#include <expected>
#include <utility>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/subcommands/init.hpp"

namespace catalyst::ide_sync {
struct Parse {
    using IdeType = catalyst::init::Parse::IdeType;

    std::vector<std::string> profiles;
    std::vector<IdeType> ides;
    bool force_emit_ide;
};
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::ide_sync
