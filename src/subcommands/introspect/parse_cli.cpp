#include <memory>
#include <utility>

#include <CLI11.hpp>

#include "catalyst/subcommands/introspect.hpp"

auto catalyst::introspect::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *introspect = app.add_subcommand("introspect", "Query resolved state from a Catalyst lifecycle hook.");
    auto result = std::make_unique<Parse>();
    introspect->add_option("path", result->path, "Dot-notation or indexed path into the hook state.");
    introspect->add_flag("-p,--pretty", result->pretty, "Pretty-print object and array values.");
    return {introspect, std::move(result)};
}
