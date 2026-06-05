#include "catalyst/dispatch.hpp"

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <CLI11.hpp>

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
}
} // namespace

namespace catalyst {

namespace {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
thread_local std::vector<std::string> g_call_chain;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

void injectCommon(std::vector<std::string> &profiles) {
    if (std::getenv("CATALYST_MACHINE"))
        return;
    if (std::ranges::find(profiles, "common") == profiles.end()) {
        profiles.insert(profiles.begin(), "common");
    }
}

template <typename ParseRes_T> int dispatchFN(const char *subc_name, const ParseRes_T &parse_res, auto fn) {
    if (std::ranges::find(g_call_chain, subc_name) != g_call_chain.end()) {
        std::string chain;
        for (const auto &n : g_call_chain)
            chain += n + " -> ";
        chain += subc_name;
        catalyst::logger.error("recursive hook detected: {}", chain);
        return 1;
    }
    if (g_call_chain.size() >= 4) {
        catalyst::logger.error("maximum hook dispatch depth exceeded");
        return 1;
    }

    struct Guard {
        ~Guard() {
            g_call_chain.pop_back();
        }
    } guard;
    g_call_chain.emplace_back(subc_name);

    catalyst::logger.debug("Executing {} subcommand", subc_name);
    try {
        if (std::expected<void, std::string> res = fn(parse_res); !res) {
            catalyst::logger.error("{}", res.error());
            return 1;
        }
    } catch (const std::exception &err) {
        catalyst::logException(err);
        return 1;
    } catch (...) {
        catalyst::logger.error("An unknown untyped exception occurred in {}", subc_name);
        return 1;
    }
    return 0;
}
} // namespace

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

std::pair<int, bool> parseCli(const std::string &args, catalyst::CliContext &ctx) {
    setupCli(ctx);

    try {
        ctx.app.parse(args, true);
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

std::pair<int, bool> parseCli(const std::vector<std::string> &args, catalyst::CliContext &ctx) {
    std::string arg_str = "catalyst ";
    for (const auto &a : args) {
        arg_str += a + " ";
    }
    return parseCli(arg_str, ctx);
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
    if (*ctx.build_subc) {
        injectCommon(ctx.build_res->profiles);
        return dispatchFN("build", *ctx.build_res, catalyst::build::action);
    }
    if (*ctx.clean_subc) {
        injectCommon(ctx.clean_res->profiles);
        return dispatchFN("clean", *ctx.clean_res, catalyst::clean::action);
    }
    if (*ctx.download_subc) {
        injectCommon(ctx.download_res->profiles);
        return dispatchFN("download", *ctx.download_res, catalyst::download::action);
    }
    if (*ctx.fetch_subc) {
        injectCommon(ctx.fetch_res->profiles);
        return dispatchFN("fetch", *ctx.fetch_res, catalyst::fetch::action);
    }
    if (*ctx.fmt_subc)
        return dispatchFN("fmt", *ctx.fmt_res, catalyst::fmt::action);
    if (*ctx.generate_subc) {
        injectCommon(ctx.generate_res->profiles);
        return dispatchFN("generate", *ctx.generate_res, catalyst::generate::action);
    }
    if (*ctx.ide_sync_subc) {
        injectCommon(ctx.ide_sync_res->profiles);
        return dispatchFN("ide-sync", *ctx.ide_sync_res, catalyst::ide_sync::action);
    }
    if (*ctx.init_subc)
        return dispatchFN("init", *ctx.init_res, catalyst::init::action);
    if (*ctx.install_subc) {
        injectCommon(ctx.install_res->profiles);
        return dispatchFN("install", *ctx.install_res, catalyst::install::action);
    }
    if (*ctx.lock_subc) {
        injectCommon(ctx.lock_res->profiles);
        return dispatchFN("lock", *ctx.lock_res, catalyst::lock::action);
    }
    if (*ctx.run_subc)
        return dispatchFN("run", *ctx.run_res, catalyst::run::action);
    if (*ctx.test_subc)
        return dispatchFN("test", *ctx.test_res, catalyst::test::action);
    if (*ctx.bench_subc)
        return dispatchFN("bench", *ctx.bench_res, catalyst::bench::action);
    if (*ctx.tidy_subc) {
        injectCommon(ctx.tidy_res->profiles);
        return dispatchFN("tidy", *ctx.tidy_res, catalyst::tidy::action);
    }
    if (*ctx.pack_subc) {
        injectCommon(ctx.pack_res->profiles);
        return dispatchFN("pack", *ctx.pack_res, catalyst::pack::action);
    }
    if (*ctx.doc_subc)
        return dispatchFN("doc", *ctx.doc_res, catalyst::doc::action);
    if (*ctx.profiles_ls_subc)
        return dispatchFN("profile-ls", *ctx.profile_ls_res, catalyst::profile_ls::action);
    if (*ctx.feature_ls_subc)
        return dispatchFN("feature-ls", *ctx.feature_ls_res, catalyst::feature_ls::action);
    catalyst::logger.info("run catalyst --help for info on available commands.");
    return 1;
}

namespace {
std::expected<void, std::string> dispatchHookImpl(auto &&args) {
    catalyst::CliContext ctx;
    ctx.workspace = catalyst::Workspace::findRoot();
    auto [exit_code, should_return] = catalyst::parseCli(args, ctx);
    if (should_return) {
        if (exit_code == 0)
            return {};
        return std::unexpected(std::format("hook parsing failed with exit code {}", exit_code));
    }

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
        std::cout << catalyst::CATALYST_VERSION << '\n';
        return {};
    }

    int disp_res = dispatch(ctx);
    if (disp_res != 0) {
        return std::unexpected(std::format("hook dispatch failed with exit code {}", disp_res));
    }
    return {};
}
} // namespace

std::expected<void, std::string> dispatchHook(const std::string &args) {
    return dispatchHookImpl(args);
}

std::expected<void, std::string> dispatchHook(const std::vector<std::string> &args) {
    return dispatchHookImpl(args);
}

} // namespace catalyst
