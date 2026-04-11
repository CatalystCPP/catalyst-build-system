#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <CLI/App.hpp>

#include "catalyst/subcommands/install.hpp"

auto catalyst::install::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *install = app.add_subcommand("install", "Install the build artifacts");
    auto ret = std::make_unique<Parse>();
    install->add_option("-p,--profiles", ret->profiles, "the profiles to compose in the build artifact")
        ->default_val(std::vector<std::string>{"common"});
    install->add_option("-s,--source", ret->source_path, "the source of the path to build")
        ->default_val(std::filesystem::current_path());
    install->add_option("-t,--target", ret->target_path, "the path to install to")->required();
    return {install, std::move(ret)};
}
