#include <expected>
#include <string>

#include <catalyst/subcommands/add.hpp>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"

#include "yaml-cpp/node/node.h"

namespace {
std::expected<void, std::string> addToProfile(const std::string &profile, const catalyst::add::system::Parse &args) {
    auto res = catalyst::utils::yaml::loadProfileFile(profile);
    if (!res) {
        return std::unexpected(res.error());
    }
    YAML::Node profile_node = res.value();

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
    new_dep["source"] = "system";
    if (!args.lib_path.empty())
        new_dep["lib"] = args.lib_path;
    if (!args.inc_path.empty())
        new_dep["include"] = args.inc_path;

    dependencies.push_back(new_dep);

    return catalyst::utils::yaml::profileWriteBack(profile, profile_node);
}
} // namespace

namespace catalyst::add::system {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &add) {
    CLI::App *add_system = add.add_subcommand("system", "add a system dependency");
    auto ret = std::make_unique<Parse>();

    add_system->add_option("name", ret->name)->required();
    add_system->add_option("-l,--lib", ret->lib_path);
    add_system->add_option("-i,--inc", ret->inc_path);
    add_system->add_option("-p,--profiles", ret->profiles);

    return {add_system, std::move(ret)};
}

std::expected<void, std::string> action(const Parse &parse_args) {
    for (const auto &profile_name : parse_args.profiles) {
        if (auto res = addToProfile(profile_name, parse_args); !res)
            return std::unexpected(res.error());
    }
    return {};
}

}; // namespace catalyst::add::system
