#pragma once

#include <expected>
#include <memory>
#include <string>

#include <CLI/App.hpp>

namespace catalyst::feature_ls {
struct Parse {
    /* void */
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &);
} // namespace catalyst::feature_ls
