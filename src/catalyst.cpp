#include <filesystem>
#include <print>
#include <utility>

#include <CLI11.hpp>

#include "catalyst/dispatch.hpp"
#include "catalyst/globals.hpp"
#include "catalyst/utils/concat_argv.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/ryml_init.hpp"

int main(int argc, char **argv) {
    catalyst::utils::yaml::installRymlErrorHandler();
    catalyst::logger.debug("{}", catalyst::concatArgv(argc, argv));

    catalyst::CliContext ctx{.workspace = catalyst::Workspace::findRoot()};
    if (auto [exit_code, should_return] = catalyst::parseCli(argc, argv, ctx); should_return)
        return exit_code;

    std::filesystem::path executable_path{argv[0]};
    if (executable_path.has_parent_path())
        executable_path = std::filesystem::absolute(executable_path);
    ctx.build_res->executable_path = std::move(executable_path);

    auto assign_ws_if = [&ctx](auto *subc, auto &res) constexpr -> bool {
        return (subc && *subc) ? (res->workspace = ctx.workspace, true) : false;
    };

    assign_ws_if(ctx.bench_subc, ctx.bench_res) || assign_ws_if(ctx.build_subc, ctx.build_res)
        || assign_ws_if(ctx.clean_subc, ctx.clean_res) || assign_ws_if(ctx.fetch_subc, ctx.fetch_res)
        || assign_ws_if(ctx.lock_subc, ctx.lock_res) || assign_ws_if(ctx.test_subc, ctx.test_res);

    if (ctx.show_version) {
        std::println("{}", catalyst::CATALYST_VERSION);
        return 0;
    }

    return dispatch(ctx);
}
