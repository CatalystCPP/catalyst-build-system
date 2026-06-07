#include <print>

#include <CLI11.hpp>

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
    // testing GH action
    catalyst::logger.debug("{}", concatArgv(argc, argv));

    catalyst::CliContext ctx{.workspace = catalyst::Workspace::findRoot()};
    if (auto [exit_code, should_return] = catalyst::parseCli(argc, argv, ctx); should_return)
        return exit_code;

    auto assign_ws_if = [&ctx](auto *subc, auto &res) {
        if (subc && *subc) {
            res->workspace = ctx.workspace;
            return true;
        }
        return false;
    };

    assign_ws_if(ctx.bench_subc, ctx.bench_res) || assign_ws_if(ctx.build_subc, ctx.build_res) ||
        assign_ws_if(ctx.clean_subc, ctx.clean_res) || assign_ws_if(ctx.fetch_subc, ctx.fetch_res) ||
        assign_ws_if(ctx.lock_subc, ctx.lock_res) || assign_ws_if(ctx.test_subc, ctx.test_res);

    if (ctx.show_version) {
        std::println("{}", catalyst::CATALYST_VERSION);
        return 0;
    }

    return dispatch(ctx);
}
