#pragma once
#include <expected>
#include <utility>

#include <CLI11.hpp>

namespace catalyst::install {
struct Parse {
    std::filesystem::path source_path;
    std::filesystem::path target_path;
    std::vector<std::string> profiles;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &);
} // namespace catalyst::install
