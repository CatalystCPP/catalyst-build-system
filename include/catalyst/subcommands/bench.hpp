#pragma once
#include <expected>
#include <string>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/workspace.hpp"

namespace catalyst::bench {
struct Parse {
    std::vector<std::string> params;
    std::optional<Workspace> workspace;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::bench
