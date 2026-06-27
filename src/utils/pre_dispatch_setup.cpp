#include <algorithm>
#include <format>
#include <functional>
#include <print>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/dispatch.hpp"
#include "catalyst/subcommands/completion.hpp"
#include "catalyst/subcommands/profile_ls.hpp"
#include "catalyst/utils/log/log.hpp"

namespace {
std::string concatArgv(int argc, char **argv) {
    std::string res;
    for (int ii = 0; ii < argc; ++ii)
        res += std::string{argv[ii]} + " ";
    return res;
}

void setupCli(catalyst::CliContext &ctx) {
    using std::tie;

    tie(ctx.add_subc, ctx.add_res) = catalyst::add::parse(ctx.app);
    tie(ctx.build_subc, ctx.build_res) = catalyst::build::parse(ctx.app);
    tie(ctx.clean_subc, ctx.clean_res) = catalyst::clean::parse(ctx.app);
    tie(ctx.download_subc, ctx.download_res) = catalyst::download::parse(ctx.app);
    tie(ctx.fetch_subc, ctx.fetch_res) = catalyst::fetch::parse(ctx.app);
    tie(ctx.fmt_subc, ctx.fmt_res) = catalyst::fmt::parse(ctx.app);
    tie(ctx.generate_subc, ctx.generate_res) = catalyst::generate::parse(ctx.app);
    tie(ctx.ide_sync_subc, ctx.ide_sync_res) = catalyst::ide_sync::parse(ctx.app);
    tie(ctx.init_subc, ctx.init_res) = catalyst::init::parse(ctx.app);
    tie(ctx.install_subc, ctx.install_res) = catalyst::install::parse(ctx.app);
    tie(ctx.lock_subc, ctx.lock_res) = catalyst::lock::parse(ctx.app);
    tie(ctx.run_subc, ctx.run_res) = catalyst::run::parse(ctx.app);
    tie(ctx.test_subc, ctx.test_res) = catalyst::test::parse(ctx.app);
    tie(ctx.bench_subc, ctx.bench_res) = catalyst::bench::parse(ctx.app);
    tie(ctx.tidy_subc, ctx.tidy_res) = catalyst::tidy::parse(ctx.app);
    tie(ctx.pack_subc, ctx.pack_res) = catalyst::pack::parse(ctx.app);
    tie(ctx.doc_subc, ctx.doc_res) = catalyst::doc::parse(ctx.app);
    tie(ctx.profiles_ls_subc, ctx.profile_ls_res) = catalyst::profile_ls::parse(ctx.app);
    tie(ctx.feature_ls_subc, ctx.feature_ls_res) = catalyst::feature_ls::parse(ctx.app);
    tie(ctx.completion_subc, ctx.completion_res) = catalyst::completion::parse(ctx.app);
    tie(ctx.add_git_subc, ctx.add_git_res) = catalyst::add::git::parse(*ctx.add_subc);
    tie(ctx.add_system_subc, ctx.add_system_res) = catalyst::add::system::parse(*ctx.add_subc);
    tie(ctx.add_local_subc, ctx.add_local_res) = catalyst::add::local::parse(*ctx.add_subc);
    tie(ctx.add_vcpkg_subc, ctx.add_vcpkg_res) = catalyst::add::vcpkg::parse(*ctx.add_subc);
    tie(ctx.add_conan_subc, ctx.add_conan_res) = catalyst::add::conan::parse(*ctx.add_subc);

    ctx.app.add_flag("-v,--version", ctx.show_version, "current version");
    ctx.app.add_flag("-V,--verbose", catalyst::logger.getVerboseLogging(), "verbose stdout logging output");
    ctx.app.footer("For more documentation, visit: https://catalystcpp.github.io/catalyst-build-system/\n\n"
                   "Copyright 2026 Siddharth Mohanty\n"
                   "Licensed under the Apache License, Version 2.0");

    ctx.app.add_subcommand("help", "Display help information for a subcommand.")->callback([&ctx]() constexpr -> void {
        std::println("{}", ctx.app.help());
        ctx.helped = true;
    });
}
} // namespace

namespace catalyst {

std::pair<int, bool> parseCli(int argc, char **argv, catalyst::CliContext &ctx) {
    using std::string_view;
    setupCli(ctx);

    try {
        ctx.app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        if (std::none_of(argv, argv + argc, [](const char *arg) {
                return string_view{arg} == "--help" || string_view{arg} == "-h";
            })) {
            catalyst::logger.error("Failed to parse provided arguments: {}", concatArgv(argc, argv));
            return {ctx.app.exit(e), true};
        }
        return {ctx.app.exit(e), true};
    }

    catalyst::logger.getVerboseLogging() = catalyst::logger.getVerboseLogging() || std::getenv("CATALYST_VERBOSE");
    // check env var for verbose logging

    if (ctx.helped)
        return {0, true};

    return {0, false};
}

std::pair<int, bool> parseCli(std::string_view args, catalyst::CliContext &ctx) {
    setupCli(ctx);

    try {
        ctx.app.parse(std::string{args}, true);
    } catch (const CLI::ParseError &e) {
        if (!args.contains("--help") && !args.contains("-h")) {
            catalyst::logger.error("Failed to parse provided arguments: {}", args);
            return {ctx.app.exit(e), true};
        }
        return {ctx.app.exit(e), true};
    }

    if (ctx.helped)
        return {0, true};

    return {0, false};
}

std::pair<int, bool> parseCli(std::span<const std::string> args, catalyst::CliContext &ctx) {
    std::string arg_str = "catalyst ";
    for (const auto &a : args) {
        arg_str += a + " ";
    }
    return parseCli(arg_str, ctx);
}
} // namespace catalyst
