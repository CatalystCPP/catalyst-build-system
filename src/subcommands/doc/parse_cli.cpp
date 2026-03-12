#include <memory>
#include <CLI/App.hpp>

#include "catalyst/subcommands/doc.hpp"

namespace catalyst::doc {

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *doc = app.add_subcommand("doc", "Build a package's documentation.");
    auto ret = std::make_unique<Parse>();
    return {doc, std::move(ret)};
}
} // namespace catalyst::doc
