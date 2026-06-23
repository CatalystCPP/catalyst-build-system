#pragma once

#include <memory>
#include <span>
#include <string_view>

#include <CLI11.hpp>

#include "catalyst/globals.hpp"
#include "catalyst/subcommands/add.hpp"
#include "catalyst/subcommands/bench.hpp"
#include "catalyst/subcommands/build.hpp"
#include "catalyst/subcommands/clean.hpp"
#include "catalyst/subcommands/completion.hpp"
#include "catalyst/subcommands/doc.hpp"
#include "catalyst/subcommands/download.hpp"
#include "catalyst/subcommands/feature_ls.hpp"
#include "catalyst/subcommands/fetch.hpp"
#include "catalyst/subcommands/fmt.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/subcommands/ide_sync.hpp"
#include "catalyst/subcommands/init.hpp"
#include "catalyst/subcommands/install.hpp"
#include "catalyst/subcommands/lock.hpp"
#include "catalyst/subcommands/pack.hpp"
#include "catalyst/subcommands/profile_ls.hpp"
#include "catalyst/subcommands/run.hpp"
#include "catalyst/subcommands/test.hpp"
#include "catalyst/subcommands/tidy.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/workspace.hpp"

namespace catalyst {
struct CliContext {
    CLI::App app{"Catalyst " + CATALYST_VERSION + " is a Modern Declarative C++ Build System."};
    std::optional<Workspace> workspace;
    bool show_version{false};
    bool helped{false};

    CLI::App *add_subc{nullptr};
    std::unique_ptr<catalyst::add::Parse> add_res{nullptr};

    CLI::App *build_subc{nullptr};
    std::unique_ptr<catalyst::build::Parse> build_res{nullptr};

    CLI::App *clean_subc{nullptr};
    std::unique_ptr<catalyst::clean::Parse> clean_res{nullptr};

    CLI::App *download_subc{nullptr};
    std::unique_ptr<catalyst::download::Parse> download_res{nullptr};

    CLI::App *fetch_subc{nullptr};
    std::unique_ptr<catalyst::fetch::Parse> fetch_res{nullptr};

    CLI::App *fmt_subc{nullptr};
    std::unique_ptr<catalyst::fmt::Parse> fmt_res{nullptr};

    CLI::App *generate_subc{nullptr};
    std::unique_ptr<catalyst::generate::Parse> generate_res{nullptr};

    CLI::App *ide_sync_subc{nullptr};
    std::unique_ptr<catalyst::ide_sync::Parse> ide_sync_res{nullptr};

    CLI::App *init_subc{nullptr};
    std::unique_ptr<catalyst::init::Parse> init_res{nullptr};

    CLI::App *install_subc{nullptr};
    std::unique_ptr<catalyst::install::Parse> install_res{nullptr};

    CLI::App *lock_subc{nullptr};
    std::unique_ptr<catalyst::lock::Parse> lock_res{nullptr};

    CLI::App *run_subc{nullptr};
    std::unique_ptr<catalyst::run::Parse> run_res{nullptr};

    CLI::App *test_subc{nullptr};
    std::unique_ptr<catalyst::test::Parse> test_res{nullptr};

    CLI::App *bench_subc{nullptr};
    std::unique_ptr<catalyst::bench::Parse> bench_res{nullptr};

    CLI::App *tidy_subc{nullptr};
    std::unique_ptr<catalyst::tidy::Parse> tidy_res{nullptr};

    CLI::App *pack_subc{nullptr};
    std::unique_ptr<catalyst::pack::Parse> pack_res{nullptr};

    CLI::App *doc_subc{nullptr};
    std::unique_ptr<catalyst::doc::Parse> doc_res{nullptr};

    CLI::App *profiles_ls_subc{nullptr};
    std::unique_ptr<catalyst::profile_ls::Parse> profile_ls_res{nullptr};

    CLI::App *feature_ls_subc{nullptr};
    std::unique_ptr<catalyst::feature_ls::Parse> feature_ls_res{nullptr};

    CLI::App *completion_subc{nullptr};
    std::unique_ptr<catalyst::completion::Parse> completion_res{nullptr};

    CLI::App *add_git_subc{nullptr};
    std::unique_ptr<catalyst::add::git::Parse> add_git_res{nullptr};

    CLI::App *add_system_subc{nullptr};
    std::unique_ptr<catalyst::add::system::Parse> add_system_res{nullptr};

    CLI::App *add_local_subc{nullptr};
    std::unique_ptr<catalyst::add::local::Parse> add_local_res{nullptr};

    CLI::App *add_vcpkg_subc{nullptr};
    std::unique_ptr<catalyst::add::vcpkg::Parse> add_vcpkg_res{nullptr};

    CLI::App *add_conan_subc{nullptr};
    std::unique_ptr<catalyst::add::conan::Parse> add_conan_res{nullptr};
};

std::pair<int, bool> parseCli(int argc, char **argv, catalyst::CliContext &ctx);
std::pair<int, bool> parseCli(std::string_view args, catalyst::CliContext &ctx);
std::pair<int, bool> parseCli(std::span<const std::string> args, catalyst::CliContext &ctx);
int dispatch(const catalyst::CliContext &ctx);
Result<void> dispatchHook(std::string_view args);
Result<void> dispatchHook(std::span<const std::string> args);
} // namespace catalyst
