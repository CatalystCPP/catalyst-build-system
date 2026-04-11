#include <algorithm>
#include <expected>
#include <string>

#include <catalyst/subcommands/add.hpp>

#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"

#include "yaml-cpp/node/node.h"

namespace {

std::expected<void, std::string> addToProfile(const std::string &profile, const catalyst::add::local::Parse &args) {
    auto profile_file = catalyst::utils::yaml::loadProfileFile(profile);
    if (!profile_file)
        return std::unexpected(profile_file.error());

    YAML::Node deps = profile_file->profile_node["dependencies"];
    const bool exists = std::ranges::any_of(
        deps, [&](const YAML::Node &d) { return d["name"] && d["name"].as<std::string>() == args.name; });
    if (exists)
        return std::unexpected("Dependency '" + args.name + "' already exists in profile '" + profile + "'.");

    YAML::Node new_dep;
    new_dep["name"] = args.name;
    new_dep["source"] = "local";
    new_dep["path"] = args.path;
    if (!args.enabled_features.empty())
        new_dep["using"] = args.enabled_features;
    deps.push_back(new_dep);

    return catalyst::utils::yaml::profileWriteBack(*profile_file);
}

} // namespace

namespace catalyst::add::local {

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    auto *cmd = app.add_subcommand("local", "add a local dependency");
    auto ret = std::make_unique<Parse>();
    cmd->add_option("name", ret->name)->required();
    cmd->add_option("path", ret->path)->required();
    cmd->add_option("-p,--profiles", ret->profiles);
    cmd->add_option("-f,--features", ret->enabled_features);
    return {cmd, std::move(ret)};
}

std::expected<void, std::string> action(const Parse &args) {
    for (const auto &profile : args.profiles)
        if (auto res = addToProfile(profile, args); !res)
            return std::unexpected(res.error());
    return {};
}

} // namespace catalyst::add::local
