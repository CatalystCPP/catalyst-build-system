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

    if (ctx.build_res)
        ctx.build_res->workspace = ctx.workspace;
    if (ctx.fetch_res)
        ctx.fetch_res->workspace = ctx.workspace;
    if (ctx.test_res)
        ctx.test_res->workspace = ctx.workspace;
    if (ctx.bench_res)
        ctx.bench_res->workspace = ctx.workspace;
    if (ctx.clean_res)
        ctx.clean_res->workspace = ctx.workspace;

    if (ctx.show_version) {
        std::println("{}", catalyst::CATALYST_VERSION);
        return 0;
    }

    return dispatch(ctx);
}
