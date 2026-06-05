#pragma once
#include <expected>
#include <string>
#include <vector>

#include <CLI11.hpp>

namespace catalyst::run {
struct Parse {
    std::string profile;
    std::vector<std::string> params;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &);
} // namespace catalyst::run
