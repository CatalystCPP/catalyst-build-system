#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"

namespace catalyst::doctor {
/** Options for the non-executing project preflight report. */
struct Parse {
    std::vector<std::string> profiles{"common"};
    std::vector<std::string> enabled_features;
    std::string backend;
    bool json{false};
    bool strict{false};
};

/** Registers doctor and its reporting/profile options. */
[[nodiscard]] std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
/** Prints all available diagnostics; fails on errors, or warnings in strict mode. */
[[nodiscard]] Result<void> action(const Parse &args);
} // namespace catalyst::doctor
