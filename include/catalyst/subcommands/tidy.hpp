#pragma once
#include <expected>
#include "catalyst/utils/result.hpp"
#include <string>

#include <CLI11.hpp>

namespace catalyst::tidy {
struct Parse {
    std::vector<std::string> profiles;
    bool fix{false};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::tidy
