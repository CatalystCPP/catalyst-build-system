#include "catalyst/dispatch.hpp"

#include <algorithm>
#include <functional>
#include <tuple>
#include <utility>

#include "catalyst/utils/log/log.hpp"

namespace {
std::string concatArgv(int argc, char **argv) {
    std::string res;
    for (int ii = 0; ii < argc; ++ii)
        res += std::string{argv[ii]} + " ";
    return res;
}
} // namespace

namespace catalyst {

namespace {
template <typename ParseRes_T> int dispatchFN(const char *subc_name, const ParseRes_T &parse_res, auto fn) {
    catalyst::logger.log(catalyst::LogLevel::DEBUG, "Executing {} subcommand", subc_name);
    if (std::expected<void, std::string> res = fn(parse_res); !res) {
        catalyst::logger.log(catalyst::LogLevel::ERROR, "{}", res.error());
        return 1;
    }
    return 0;
}
} // namespace

std::pair<int, bool> parseCli(int argc, char **argv, catalyst::CliContext &ctx) {
    using std::tie, std::string_view;

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
    // tie(ctx.bench_subc, ctx.bench_res) = catalyst::bench::parse(ctx.app);
    tie(ctx.doc_subc, ctx.doc_res) = catalyst::doc::parse(ctx.app);
    tie(ctx.add_git_subc, ctx.add_git_res) = catalyst::add::git::parse(*ctx.add_subc);
    tie(ctx.add_system_subc, ctx.add_system_res) = catalyst::add::system::parse(*ctx.add_subc);
    tie(ctx.add_local_subc, ctx.add_local_res) = catalyst::add::local::parse(*ctx.add_subc);
    tie(ctx.add_vcpkg_subc, ctx.add_vcpkg_res) = catalyst::add::vcpkg::parse(*ctx.add_subc);

    ctx.app.add_flag("-v,--version", ctx.show_version, "current version");
    ctx.app.add_flag("-V,--verbose", catalyst::logger.getVerboseLogging(), "verbose stdout logging output");
    ctx.app.footer("For more documentation, visit: https://catalystcpp.github.io/catalyst-build-system/\n\n"
                   "Copyright 2026 Siddharth Mohanty\n"
                   "Licensed under the Apache License, Version 2.0");

    ctx.app.add_subcommand("help", "Display help information for a subcommand.")->callback([&]() {
        std::cout << ctx.app.help() << '\n';
        ctx.helped = true;
    });

    try {
        ctx.app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        if (std::none_of(argv, argv + argc, [](const char *arg) {
                return string_view{arg} == "--help" || string_view{arg} == "-h";
            })) {
            catalyst::logger.log(
                catalyst::LogLevel::ERROR, "Failed to parse provided arguments: {}", concatArgv(argc, argv));
            return {ctx.app.exit(e), true};
        }
        return {ctx.app.exit(e), true};
    }

    if (ctx.helped)
        return {0, true};

    return {0, false};
}

int dispatch(const catalyst::CliContext &ctx) {
    if (*ctx.add_subc) {
        if (*ctx.add_git_subc)
            return dispatchFN("add git", *ctx.add_git_res, catalyst::add::git::action);
        if (*ctx.add_system_subc)
            return dispatchFN("add system", *ctx.add_system_res, catalyst::add::system::action);
        if (*ctx.add_local_subc)
            return dispatchFN("add local", *ctx.add_local_res, catalyst::add::local::action);
        if (*ctx.add_vcpkg_subc)
            return dispatchFN("add vcpkg", *ctx.add_vcpkg_res, catalyst::add::vcpkg::action);
        return 1;
    }
    if (*ctx.build_subc)
        return dispatchFN("build", *ctx.build_res, catalyst::build::action);
    if (*ctx.clean_subc)
        return dispatchFN("clean", *ctx.clean_res, catalyst::clean::action);
    if (*ctx.download_subc)
        return dispatchFN("download", *ctx.download_res, catalyst::download::action);
    if (*ctx.fetch_subc)
        return dispatchFN("fetch", *ctx.fetch_res, catalyst::fetch::action);
    if (*ctx.fmt_subc)
        return dispatchFN("fmt", *ctx.fmt_res, catalyst::fmt::action);
    if (*ctx.generate_subc)
        return dispatchFN("generate", *ctx.generate_res, catalyst::generate::action);
    if (*ctx.ide_sync_subc)
        return dispatchFN("ide_sync", *ctx.ide_sync_res, catalyst::ide_sync::action);
    if (*ctx.init_subc)
        return dispatchFN("init", *ctx.init_res, catalyst::init::action);
    if (*ctx.install_subc)
        return dispatchFN("install", *ctx.install_res, catalyst::install::action);
    if (*ctx.lock_subc)
        return dispatchFN("lock", *ctx.lock_res, catalyst::lock::action);
    if (*ctx.run_subc)
        return dispatchFN("run", *ctx.run_res, catalyst::run::action);
    if (*ctx.test_subc)
        return dispatchFN("test", *ctx.test_res, catalyst::test::action);
    if (*ctx.bench_subc)
        return dispatchFN("bench", *ctx.bench_res, catalyst::bench::action);
    if (*ctx.tidy_subc)
        return dispatchFN("tidy", *ctx.tidy_res, catalyst::tidy::action);
    if (*ctx.pack_subc)
        return dispatchFN("pack", *ctx.pack_res, catalyst::pack::action);
    if (*ctx.doc_subc)
        return dispatchFN("doc", *ctx.doc_res, catalyst::doc::action);
    catalyst::logger.log(catalyst::LogLevel::ERROR, "run catalyst --help for info on available commands.");
    return 1;
}
} // namespace catalyst
