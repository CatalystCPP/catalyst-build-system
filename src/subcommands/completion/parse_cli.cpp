#include <memory>
#include <string>

#include <CLI11.hpp>

#include "catalyst/subcommands/completion.hpp"

auto catalyst::completion::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *completion = app.add_subcommand("completion", "Generate shell completion scripts.");
    auto ret = std::make_unique<Parse>();

    completion->add_option("shell", ret->shell, "The shell to generate completion for (bash, zsh)")
        ->required()
        ->check(CLI::IsMember({"bash", "zsh"}));

    return {completion, std::move(ret)};
}
