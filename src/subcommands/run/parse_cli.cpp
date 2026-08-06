#include "catalyst/subcommands/run.hpp"

auto catalyst::run::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *run = app.add_subcommand("run", "Run a built executable.");
    auto ret = std::make_unique<Parse>();
    run->add_option("-p,--profiles", ret->profiles, "Profile composition to run.")
        ->default_val(std::vector<std::string>{"common"});
    run->add_option("-P,--params", ret->params);
    run->prefix_command(CLI::PrefixCommandMode::SeparatorOnly);
    run->parse_complete_callback([run, parse_result = ret.get()]() -> void {
        std::vector<std::string> passthrough = run->remaining();
        if (!passthrough.empty() && passthrough.front() == "--") {
            passthrough.erase(passthrough.begin());
        }
        parse_result->params.insert(parse_result->params.end(), passthrough.begin(), passthrough.end());
    });
    return {run, std::move(ret)};
}
