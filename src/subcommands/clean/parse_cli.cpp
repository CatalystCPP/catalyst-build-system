#include "catalyst/subcommands/clean.hpp"

auto catalyst::clean::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *clean = app.add_subcommand("clean", "Clean artifacts.");
    auto ret = std::make_unique<Parse>();
    clean->add_option("-p,--profile", ret->profiles);
    clean->add_flag("-i,--intermediates", ret->intermediates, "Clean intermediate files only.");
    return {clean, std::move(ret)};
}
