#pragma once
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include <CLI11.hpp>
#include <ryml/ryml.hpp>

#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/workspace.hpp"

namespace catalyst::fetch {
struct Parse {
    std::vector<std::string> profiles;
    std::optional<Workspace> workspace;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &);
std::expected<void, std::string> fetchConanDeps(
    const std::vector<ryml::ConstNodeRef> &conan_deps, ///< util because conan needs to be fed the compiler and flags
    const std::string &build_dir,
    const utils::yaml::Configuration &config,
    const std::vector<std::string> &profiles);
} // namespace catalyst::fetch
