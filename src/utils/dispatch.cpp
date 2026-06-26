#include "catalyst/dispatch.hpp"

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/os/os_defs.hpp"

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

template <typename ParseRes_T> int dispatchFN(const char *subc_name, const ParseRes_T &parse_res, auto fn) {
    if (std::ranges::find(g_call_chain, subc_name) != g_call_chain.end()) {
        std::string chain;
        for (const auto &n : g_call_chain)
            chain += n + " -> ";
        chain += subc_name;
        catalyst::logger.error("recursive hook detected: {}", chain);
        return 1;
    }
    constexpr auto TUNABLE_MAX_DEPTH = 8;
    if (g_call_chain.size() >= TUNABLE_MAX_DEPTH) {
        catalyst::logger.error("maximum hook dispatch depth ({}) exceeded", TUNABLE_MAX_DEPTH);
        return 1;
    }

    struct Guard {
        Guard() = default;
        Guard(const Guard &) = delete;
        Guard(Guard &&) = delete;
        Guard &operator=(const Guard &) = delete;
        Guard &operator=(Guard &&) = delete;
        ~Guard() {
            g_call_chain.pop_back();
        }
    } guard;
    g_call_chain.emplace_back(subc_name);

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
            return dispatchFN("add git", *ctx.add_git_res, catalyst::add::git::action);
        if (*ctx.add_system_subc)
            return dispatchFN("add system", *ctx.add_system_res, catalyst::add::system::action);
        if (*ctx.add_local_subc)
            return dispatchFN("add local", *ctx.add_local_res, catalyst::add::local::action);
        if (*ctx.add_vcpkg_subc)
            return dispatchFN("add vcpkg", *ctx.add_vcpkg_res, catalyst::add::vcpkg::action);
        if (*ctx.add_conan_subc)
            return dispatchFN("add conan", *ctx.add_conan_res, catalyst::add::conan::action);
        return 1;
    }
    if (*ctx.build_subc) {
        checkRequiredTools();
        injectCommon(ctx.build_res->profiles);
        return dispatchFN("build", *ctx.build_res, catalyst::build::action);
    }
    if (*ctx.clean_subc) {
        checkRequiredTools();
        injectCommon(ctx.clean_res->profiles);
        return dispatchFN("clean", *ctx.clean_res, catalyst::clean::action);
    }
    if (*ctx.download_subc) {
        checkRequiredTools();
        injectCommon(ctx.download_res->profiles);
        return dispatchFN("download", *ctx.download_res, catalyst::download::action);
    }
    if (*ctx.fetch_subc) {
        checkRequiredTools();
        injectCommon(ctx.fetch_res->profiles);
        return dispatchFN("fetch", *ctx.fetch_res, catalyst::fetch::action);
    }
    if (*ctx.fmt_subc) {
        checkRequiredTools();
        return dispatchFN("fmt", *ctx.fmt_res, catalyst::fmt::action);
    }
    if (*ctx.generate_subc) {
        checkRequiredTools();
        injectCommon(ctx.generate_res->profiles);
        return dispatchFN("generate", *ctx.generate_res, catalyst::generate::action);
    }
    if (*ctx.ide_sync_subc) {
        injectCommon(ctx.ide_sync_res->profiles);
        return dispatchFN("ide-sync", *ctx.ide_sync_res, catalyst::ide_sync::action);
    }
    if (*ctx.init_subc) {
        return dispatchFN("init", *ctx.init_res, catalyst::init::action);
    }
    if (*ctx.install_subc) {
        checkRequiredTools();
        injectCommon(ctx.install_res->profiles);
        return dispatchFN("install", *ctx.install_res, catalyst::install::action);
    }
    if (*ctx.lock_subc) {
        checkRequiredTools();
        injectCommon(ctx.lock_res->profiles);
        return dispatchFN("lock", *ctx.lock_res, catalyst::lock::action);
    }
    if (*ctx.run_subc) {
        checkRequiredTools();
        return dispatchFN("run", *ctx.run_res, catalyst::run::action);
    }
    if (*ctx.test_subc) {
        checkRequiredTools();
        return dispatchFN("test", *ctx.test_res, catalyst::test::action);
    }
    if (*ctx.bench_subc) {
        checkRequiredTools();
        return dispatchFN("bench", *ctx.bench_res, catalyst::bench::action);
    }
    if (*ctx.tidy_subc) {
        checkRequiredTools();
        injectCommon(ctx.tidy_res->profiles);
        return dispatchFN("tidy", *ctx.tidy_res, catalyst::tidy::action);
    }
    if (*ctx.pack_subc) {
        checkRequiredTools();
        injectCommon(ctx.pack_res->profiles);
        return dispatchFN("pack", *ctx.pack_res, catalyst::pack::action);
    }
    if (*ctx.doc_subc) {
        checkRequiredTools();
        return dispatchFN("doc", *ctx.doc_res, catalyst::doc::action);
    }
    if (*ctx.profiles_ls_subc) {
        return dispatchFN("profile-ls", *ctx.profile_ls_res, catalyst::profile_ls::action);
    }
    if (*ctx.feature_ls_subc) {
        checkRequiredTools();
        return dispatchFN("feature-ls", *ctx.feature_ls_res, catalyst::feature_ls::action);
    }
    if (*ctx.completion_subc) {
        return dispatchFN("completion", *ctx.completion_res, [&](const auto &args) {
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

Result<void> dispatchHook(std::string_view args) {
    return dispatchHookImpl(args);
}

Result<void> dispatchHook(std::span<const std::string> args) {
    return dispatchHookImpl(args);
}
} // namespace catalyst
