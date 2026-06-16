#pragma once
#include <expected>
#include <string>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/workspace.hpp"

namespace catalyst::clean {
struct Parse {
    std::vector<std::string> profiles{"common"};
    std::optional<Workspace> workspace;
    bool intermediates{false};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &parse_args);
} // namespace catalyst::clean
