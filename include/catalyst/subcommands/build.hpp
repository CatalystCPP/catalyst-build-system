#pragma once
#include <expected>
#include <filesystem>
#include <string>
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
    std::string package;
    std::vector<std::string> profiles;
    std::vector<std::string> enabled_features;
    std::string backend;
    std::optional<Workspace> workspace;
    std::filesystem::path executable_path{"catalyst"};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::build
