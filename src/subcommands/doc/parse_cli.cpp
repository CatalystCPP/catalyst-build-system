#include <memory>
#include <CLI/App.hpp>

#include "catalyst/subcommands/doc.hpp"

namespace catalyst::doc {

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *doc = app.add_subcommand("doc", "Build a package's documentation.");
    auto ret = std::make_unique<Parse>();
    
    doc->add_option("-e,--engine", ret->engine, "Override the documentation engine (doxygen, clang-doc, jocasta)");
    doc->add_flag("--open", ret->open, "Open the generated documentation in the default browser upon completion");
    doc->add_flag("--clean", ret->clean, "Clean the existing output directory before generation");
    
    return {doc, std::move(ret)};
}
} // namespace catalyst::doc
