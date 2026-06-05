#pragma once
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/workspace.hpp"

namespace catalyst::fetch {
struct Parse {
    std::vector<std::string> profiles;
    std::optional<Workspace> workspace;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &);
} // namespace catalyst::fetch
