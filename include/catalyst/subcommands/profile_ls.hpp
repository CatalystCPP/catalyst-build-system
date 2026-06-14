#pragma once

#include <expected>
#include "catalyst/utils/result.hpp"
#include <utility>

#include <CLI11.hpp>

namespace catalyst::profile_ls {
struct Parse {
    /* void */
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::profile_ls
