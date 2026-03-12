#pragma once
#include <expected>
#include <string>

#include <CLI/App.hpp>

namespace catalyst::tidy {
struct Parse {
    std::vector<std::string> profiles;
    bool fix{false};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &);
} // namespace catalyst::tidy
