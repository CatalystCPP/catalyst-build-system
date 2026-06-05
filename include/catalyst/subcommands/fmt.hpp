#pragma once
#include <expected>
#include <string>
#include <vector>

#include <CLI11.hpp>

namespace catalyst::fmt {
struct Parse {
    std::vector<std::string> profiles{"common"};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &parse_args);
} // namespace catalyst::fmt
