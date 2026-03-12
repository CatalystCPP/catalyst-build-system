#pragma once
#include <expected>
#include <string>

#include <CLI/App.hpp>

namespace catalyst::doc {
struct Parse {
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &parse_args);
} // namespace catalyst::doc
