#pragma once
#include <expected>
#include "catalyst/utils/result.hpp"
#include <utility>

#include <CLI11.hpp>

namespace catalyst::download {
struct Parse {
    std::string git_remote;
    std::string git_branch;
    std::filesystem::path target_path;
    std::vector<std::string> profiles;
    std::vector<std::string> enabled_features;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::download
