#include <expected>
#include <string>
#include <vector>

#include <catalyst/subcommands/add.hpp>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"

#include "yaml-cpp/node/node.h"

namespace {
std::expected<void, std::string> addToProfile(const std::string &profile, const catalyst::add::vcpkg::Parse &args) {
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

    bool dependency_found = false;
    for (const auto &dep : dependencies) {
        if (dep["name"] && dep["name"].as<std::string>() == args.name) {
            dependency_found = true;
            break;
        }
    }

    if (dependency_found) {
        return std::unexpected("Dependency '" + args.name + "' already exists in profile '" + profile + "'.");
    }

    YAML::Node new_dep;
    new_dep["name"] = args.name;
    new_dep["source"] = "vcpkg";
    if (!args.triplet.empty()) {
        new_dep["triplet"] = args.triplet;
    }
    if (!args.version.empty()) {
        new_dep["version"] = args.version;
    } else {
        new_dep["version"] = "latest";
    }
    if (!args.enabled_features.empty()) {
        new_dep["using"] = args.enabled_features;
    }

    dependencies.push_back(new_dep);

    return catalyst::utils::yaml::profileWriteBack(profile_file);
}
} // namespace

namespace catalyst::add::vcpkg {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &add) {
    CLI::App *add_vcpkg = add.add_subcommand("vcpkg", "add a vcpkg dependency");
    auto ret = std::make_unique<Parse>();

    add_vcpkg->add_option("name", ret->name)->required();
    add_vcpkg->add_option("-t,--triplet", ret->triplet)->required();
    add_vcpkg->add_option("-v,--version", ret->version)->default_str("latest");
    add_vcpkg->add_option("-p,--profiles", ret->profiles)->default_val(std::vector<std::string>{"common"});
    add_vcpkg->add_option("-f,--features", ret->enabled_features);

    return {add_vcpkg, std::move(ret)};
}

std::expected<void, std::string> action(const Parse &parse_args) {
    for (const auto &profile_name : parse_args.profiles) {
        if (auto res = addToProfile(profile_name, parse_args); !res)
            return std::unexpected(res.error());
    }
    return {};
}

} // namespace catalyst::add::vcpkg
