#include "catalyst/dispatch.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <print>
#include <string>
#include <vector>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/os/os_defs.hpp"
#include "catalyst/utils/runtime_context.hpp"

static constexpr size_t TUNABLE_MAX_HOOK_DEPTH = 8Z;

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

void checkRequiredTools() {
    if (!catalyst::utils::os::isCommandInstalled("vcpkg")) {
        catalyst::logger.warn("vcpkg is not installed or not in PATH.");
    }
    if (!catalyst::utils::os::isCommandInstalled("git")) {
        catalyst::logger.warn("git is not installed or not in PATH.");
    }
    if (!catalyst::utils::os::isCommandInstalled("pkg-config")) {
        catalyst::logger.warn("pkg-config is not installed or not in PATH.");
    }
    if (!catalyst::utils::os::isCommandInstalled("conan")) {
        catalyst::logger.warn("conan is not installed or not in PATH.");
    }
}

template <typename ParseRes_T>
int dispatchSubcommand(const std::string_view &&subc_name, const ParseRes_T &parse_res, auto fn) {
    if (std::ranges::find(g_call_chain, subc_name) != g_call_chain.end()) {
        std::string chain;
        for (const auto &n : g_call_chain)
            chain += n + " -> ";
        chain += subc_name;
        catalyst::logger.error("recursive hook detected: {}", chain);
        return 1;
    }
    if (g_call_chain.size() >= TUNABLE_MAX_HOOK_DEPTH) {
        catalyst::logger.error("maximum hook dispatch depth ({}) exceeded", TUNABLE_MAX_HOOK_DEPTH);
        return 1;
    }

    struct Guard {
        std::vector<std::string> previous_features;

        Guard() = default;
        Guard(const Guard &) = delete;
        Guard(Guard &&) = delete;
        Guard &operator=(const Guard &) = delete;
        Guard &operator=(Guard &&) = delete;
        ~Guard() {
            catalyst::utils::runtime::setEnabledFeatures(previous_features);
            g_call_chain.pop_back();
        }
    } guard;
    guard.previous_features = catalyst::utils::runtime::enabledFeatures();
    g_call_chain.emplace_back(subc_name);

    if constexpr (requires { parse_res.enabled_features; }) {
        catalyst::utils::runtime::setEnabledFeatures(parse_res.enabled_features);
    } else {
        catalyst::utils::runtime::setEnabledFeatures({});
    }

    catalyst::logger.debug("Executing {} subcommand", subc_name);
    try {
        if (Result<void> res = fn(parse_res); !res) {
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

int dispatch(const catalyst::CliContext &ctx) {
    if (*ctx.add_subc) {
        checkRequiredTools();
        if (*ctx.add_git_subc)
            return dispatchSubcommand("add git", *ctx.add_git_res, catalyst::add::git::action);
        if (*ctx.add_system_subc)
            return dispatchSubcommand("add system", *ctx.add_system_res, catalyst::add::system::action);
        if (*ctx.add_local_subc)
            return dispatchSubcommand("add local", *ctx.add_local_res, catalyst::add::local::action);
        if (*ctx.add_vcpkg_subc)
            return dispatchSubcommand("add vcpkg", *ctx.add_vcpkg_res, catalyst::add::vcpkg::action);
        if (*ctx.add_conan_subc)
            return dispatchSubcommand("add conan", *ctx.add_conan_res, catalyst::add::conan::action);
        return 1;
    }
    if (*ctx.build_subc) {
        checkRequiredTools();
        injectCommon(ctx.build_res->profiles);
        return dispatchSubcommand("build", *ctx.build_res, catalyst::build::action);
    }
    if (*ctx.clean_subc) {
        checkRequiredTools();
        injectCommon(ctx.clean_res->profiles);
        return dispatchSubcommand("clean", *ctx.clean_res, catalyst::clean::action);
    }
    if (*ctx.download_subc) {
        checkRequiredTools();
        injectCommon(ctx.download_res->profiles);
        return dispatchSubcommand("download", *ctx.download_res, catalyst::download::action);
    }
    if (*ctx.fetch_subc) {
        checkRequiredTools();
        injectCommon(ctx.fetch_res->profiles);
        return dispatchSubcommand("fetch", *ctx.fetch_res, catalyst::fetch::action);
    }
    if (*ctx.fmt_subc) {
        checkRequiredTools();
        return dispatchSubcommand("fmt", *ctx.fmt_res, catalyst::fmt::action);
    }
    if (*ctx.generate_subc) {
        checkRequiredTools();
        injectCommon(ctx.generate_res->profiles);
        return dispatchSubcommand("generate", *ctx.generate_res, catalyst::generate::action);
    }
    if (*ctx.ide_sync_subc) {
        injectCommon(ctx.ide_sync_res->profiles);
        return dispatchSubcommand("ide-sync", *ctx.ide_sync_res, catalyst::ide_sync::action);
    }
    if (*ctx.init_subc) {
        return dispatchSubcommand("init", *ctx.init_res, catalyst::init::action);
    }
    if (*ctx.install_subc) {
        checkRequiredTools();
        injectCommon(ctx.install_res->profiles);
        return dispatchSubcommand("install", *ctx.install_res, catalyst::install::action);
    }
    if (*ctx.introspect_subc) {
        return dispatchSubcommand("introspect", *ctx.introspect_res, catalyst::introspect::action);
    }
    if (*ctx.lock_subc) {
        checkRequiredTools();
        injectCommon(ctx.lock_res->profiles);
        return dispatchSubcommand("lock", *ctx.lock_res, catalyst::lock::action);
    }
    if (*ctx.run_subc) {
        checkRequiredTools();
        return dispatchSubcommand("run", *ctx.run_res, catalyst::run::action);
    }
    if (*ctx.test_subc) {
        checkRequiredTools();
        return dispatchSubcommand("test", *ctx.test_res, catalyst::test::action);
    }
    if (*ctx.bench_subc) {
        checkRequiredTools();
        return dispatchSubcommand("bench", *ctx.bench_res, catalyst::bench::action);
    }
    if (*ctx.tidy_subc) {
        checkRequiredTools();
        injectCommon(ctx.tidy_res->profiles);
        return dispatchSubcommand("tidy", *ctx.tidy_res, catalyst::tidy::action);
    }
    if (*ctx.pack_subc) {
        checkRequiredTools();
        injectCommon(ctx.pack_res->profiles);
        return dispatchSubcommand("pack", *ctx.pack_res, catalyst::pack::action);
    }
    if (*ctx.doc_subc) {
        checkRequiredTools();
        return dispatchSubcommand("doc", *ctx.doc_res, catalyst::doc::action);
    }
    if (*ctx.profiles_ls_subc) {
        return dispatchSubcommand("profile-ls", *ctx.profile_ls_res, catalyst::profile_ls::action);
    }
    if (*ctx.feature_ls_subc) {
        checkRequiredTools();
        return dispatchSubcommand("feature-ls", *ctx.feature_ls_res, catalyst::feature_ls::action);
    }
    if (*ctx.completion_subc) {
        return dispatchSubcommand("completion", *ctx.completion_res, [&](const auto &args) {
            return catalyst::completion::action(args, ctx.app);
        });
    }
    catalyst::logger.info("run catalyst --help for info on available commands.");
    return 1;
}

namespace {
Result<void> dispatchHookImpl(auto &&args) {
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

    assign_ws_if(ctx.bench_subc, ctx.bench_res) || assign_ws_if(ctx.build_subc, ctx.build_res)
        || assign_ws_if(ctx.clean_subc, ctx.clean_res) || assign_ws_if(ctx.fetch_subc, ctx.fetch_res)
        || assign_ws_if(ctx.lock_subc, ctx.lock_res) || assign_ws_if(ctx.test_subc, ctx.test_res);

    if (ctx.show_version) {
        std::println("{}", catalyst::CATALYST_VERSION);
        return {};
    }

    int disp_res = dispatch(ctx);
    if (disp_res != 0) {
        return std::unexpected(std::format("hook dispatch failed with exit code {}", disp_res));
    }
    return {};
}
} // namespace

Result<void> dispatchHook(std::string_view args) {
    return dispatchHookImpl(args);
}

Result<void> dispatchHook(std::span<const std::string> args) {
    return dispatchHookImpl(args);
}
} // namespace catalyst
