#pragma once

#include <memory>
#include <string>
#include <utility>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"

namespace catalyst::introspect {

struct Parse {
    std::string path;
    bool pretty{false};
};

[[nodiscard]] std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
[[nodiscard]] Result<void> action(const Parse &parse_res);

} // namespace catalyst::introspect
