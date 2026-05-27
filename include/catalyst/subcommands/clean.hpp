#pragma once
#include <expected>
#include <string>
#include <vector>

#include <CLI/App.hpp>

#include "catalyst/workspace.hpp"

namespace catalyst::clean {
struct Parse {
    std::vector<std::string> profiles{"common"};
    std::optional<Workspace> workspace;
    bool intermediates{false};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &parse_args);
} // namespace catalyst::clean
