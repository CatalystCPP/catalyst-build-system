#pragma once

#include <expected>
#include <utility>

#include <CLI11.hpp>

namespace catalyst::profile_ls {
struct Parse {
    /* void */
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &);
} // namespace catalyst::profile_ls
