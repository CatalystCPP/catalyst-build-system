#pragma once

#include <expected>
#include <memory>
#include <string>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"

namespace catalyst::feature_ls {
struct Parse {
    bool inverse{false};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::feature_ls
