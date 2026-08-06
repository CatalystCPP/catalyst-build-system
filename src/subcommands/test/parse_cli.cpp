#include "catalyst/subcommands/test.hpp"

auto catalyst::test::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *test = app.add_subcommand("test", "Run the test executable.");
    auto ret = std::make_unique<Parse>();
    test->add_option("-P,--params", ret->params, "Params to pass to the test executable.");
    test->add_flag("-r,--rebuild", ret->rebuild, "Rebuild before testing.")->default_val(false);
    test->prefix_command(CLI::PrefixCommandMode::SeparatorOnly);
    test->parse_complete_callback([test, parse_result = ret.get()]() -> void {
        std::vector<std::string> passthrough = test->remaining();
        if (!passthrough.empty() && passthrough.front() == "--") {
            passthrough.erase(passthrough.begin());
        }
        parse_result->params.insert(parse_result->params.end(), passthrough.begin(), passthrough.end());
    });
    return {test, std::move(ret)};
}
