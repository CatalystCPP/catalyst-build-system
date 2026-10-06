#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/workspace.hpp"

namespace catalyst::build {
struct Parse {
    bool regen;
    bool force_rebuild;
    bool force_refetch;
    bool workspace_build;
    bool watch;
    bool explain = false;
    std::string package;
    std::vector<std::string> profiles;
    std::vector<std::string> enabled_features;
    std::string backend;
    std::optional<Workspace> workspace;
    std::filesystem::path executable_path{"catalyst"};
    /// Registered CLI options, used to report which values were supplied versus defaulted.
    /// The options are owned by the CLI::App, which outlives every build action.
    std::vector<std::pair<std::string, const CLI::Option *>> cli_options;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
/// Human-readable representation of the parsed build options for explanation output.
std::string describe(const Parse &);
} // namespace catalyst::build
