#include <expected>
#include <string>

#include <catalyst/subcommands/add.hpp>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"

#include "yaml-cpp/node/node.h"

namespace {
std::expected<void, std::string> addToProfile(const std::string &profile, const catalyst::add::git::Parse &args) {
    auto res = catalyst::utils::yaml::loadProfileFile(profile);
    if (!res) {
        return std::unexpected(res.error());
    }
    auto profile_file = res.value();
    YAML::Node profile_node = profile_file.profile_node;

    if (!profile_node["dependencies"]) {
        profile_node["dependencies"] = YAML::Node(YAML::NodeType::Sequence);
    }

    YAML::Node dependencies = profile_node["dependencies"];

    // Parse name from URL
    std::string name;
    if (!args.name.empty()) {
        name = args.name;
    } else {
        if (!args.remote.empty()) {
            auto pos = args.remote.find_last_of('/');
            name = (pos == std::string::npos) ? args.remote : args.remote.substr(pos + 1);
            if (name.ends_with(".git")) {
                name.resize(name.length() - 4);
            }
        }

        if (name.empty()) {
            return std::unexpected("Could not derive dependency name from URL: " + args.remote);
        }
    }

    bool dependency_found = false;
    for (const auto &dep : dependencies) {
        if (dep["name"] && dep["name"].as<std::string>() == name) {
            dependency_found = true;
            break;
        }
    }

    if (dependency_found) {
        return std::unexpected("Dependency '" + name + "' already exists in profile '" + profile + "'.");
    }

    YAML::Node new_dep;
    new_dep["name"] = name;
    new_dep["source"] = "git";
    new_dep["url"] = args.remote;
    new_dep["version"] = args.version;
    if (!args.enabled_features.empty()) {
        new_dep["using"] = args.enabled_features;
    }

    dependencies.push_back(new_dep);

    return catalyst::utils::yaml::profileWriteBack(profile_file);
}
} // namespace

namespace catalyst::add::git {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &add) {
    CLI::App *add_git = add.add_subcommand("git", "add a remote git dependency");
    auto ret = std::make_unique<Parse>();

    add_git->add_option("remote", ret->remote);
    add_git->add_option("-n,--name", ret->name);
    add_git->add_option("-v,--version", ret->version)->default_val("latest");
    add_git->add_option("-f,--features", ret->enabled_features);
    add_git->add_option("-p,--profiles", ret->profiles);

    return {add_git, std::move(ret)};
}

std::expected<void, std::string> action(const Parse &parse_args) {
    for (const auto &profile_name : parse_args.profiles) {
        if (auto res = addToProfile(profile_name, parse_args); !res)
            return std::unexpected(res.error());
    }
    return {};
}

}; // namespace catalyst::add::git
