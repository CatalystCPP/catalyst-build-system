#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include <CLI/App.hpp>

#include "catalyst/workspace.hpp"

namespace catalyst::lock {
struct Parse {
    std::vector<std::string> profiles;
    std::optional<catalyst::Workspace> workspace;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &parse_args);
} // namespace catalyst::lock
