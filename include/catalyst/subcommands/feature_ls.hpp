#pragma once

#include <expected>
#include <memory>
#include <string>

#include <CLI11.hpp>

namespace catalyst::feature_ls {
struct Parse {
    bool inverse{false};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &);
} // namespace catalyst::feature_ls
