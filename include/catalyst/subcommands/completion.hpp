#pragma once
#include <memory>
#include <string>
#include <utility>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"

namespace catalyst::completion {
struct Parse {
    std::string shell;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &parse_args, const CLI::App &app);
} // namespace catalyst::completion
