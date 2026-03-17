#include <format>
#include <print>
#include <string>

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>

#include "catalyst/dispatch.hpp"
#include "catalyst/globals.hpp"
#include "catalyst/utils/log/log.hpp"

namespace {
std::string concatArgv(int argc, char **argv) {
    std::string res;
    for (int ii = 0; ii < argc; ++ii)
        res += std::string{argv[ii]} + " ";
    return res;
}
} // namespace

int main(int argc, char **argv) {
    std::string args_str = concatArgv(argc, argv);
    catalyst::logger.log(catalyst::LogLevel::DEBUG, "{}", args_str);

    catalyst::CliContext ctx;
    ctx.workspace = catalyst::Workspace::findRoot();
    auto [exit_code, should_return] = catalyst::parseCli(argc, argv, ctx);
    if (should_return)
        return exit_code;

    auto assign_ws_if = [&ctx](auto &res) {
        if (res)
            res->workspace = ctx.workspace;
    };

    assign_ws_if(ctx.bench_res);
    assign_ws_if(ctx.build_res);
    assign_ws_if(ctx.clean_res);
    assign_ws_if(ctx.fetch_res);
    assign_ws_if(ctx.lock_res);
    assign_ws_if(ctx.test_res);

    if (ctx.show_version) {
        std::println("{}", catalyst::CATALYST_VERSION);
        return 0;
    }

    return dispatch(ctx);
}
