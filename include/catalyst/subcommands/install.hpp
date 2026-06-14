#pragma once
#include <expected>
#include "catalyst/utils/result.hpp"
#include <utility>

#include <CLI11.hpp>

namespace catalyst::install {
struct Parse {
    std::filesystem::path source_path;
    std::filesystem::path target_path;
    std::vector<std::string> profiles;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::install
