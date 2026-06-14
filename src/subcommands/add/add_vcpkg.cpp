#include <expected>
#include <string>
#include <vector>

#include <catalyst/subcommands/add.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

using catalyst::Result;

namespace {
Result<void> addToProfile(const std::string &profile, const catalyst::add::vcpkg::Parse &args) {
    namespace yaml = catalyst::utils::yaml;
    auto res = yaml::loadProfileFile(profile);
    if (!res) {
        return std::unexpected(res.error());
    }
    yaml::ProfileFile &profile_file = res.value();
    ryml::Tree &tree = profile_file.tree;

    ryml::NodeRef dependencies = yaml::childOrCreate(profile_file.profile(), "dependencies");
    dependencies |= ryml::SEQ;

    bool dependency_found = false;
    for (ryml::ConstNodeRef dep : dependencies.children()) {
        if (yaml::asString(yaml::child(dep, "name")) == args.name) {
            dependency_found = true;
            break;
        }
    }

    if (dependency_found) {
        return std::unexpected("Dependency '" + args.name + "' already exists in profile '" + profile + "'.");
    }

    ryml::NodeRef new_dep = dependencies.append_child();
    new_dep |= ryml::MAP;
    new_dep["name"] = tree.to_arena(args.name);
    new_dep["source"] = "vcpkg";
    if (!args.triplet.empty()) {
        new_dep["triplet"] = tree.to_arena(args.triplet);
    }
    if (!args.version.empty()) {
        new_dep["version"] = tree.to_arena(args.version);
    } else {
        new_dep["version"] = "latest";
    }
    if (!args.enabled_features.empty()) {
        ryml::NodeRef using_node = yaml::childOrCreate(new_dep, "using");
        using_node |= ryml::SEQ;
        for (const auto &feature : args.enabled_features)
            using_node.append_child() = tree.to_arena(feature);
    }

    return yaml::profileWriteBack(profile_file);
}
} // namespace

namespace catalyst::add::vcpkg {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &add) {
    CLI::App *add_vcpkg = add.add_subcommand("vcpkg", "add a vcpkg dependency");
    auto ret = std::make_unique<Parse>();

    add_vcpkg->add_option("-n,--name", ret->name)->required();
    add_vcpkg->add_option("-t,--triplet", ret->triplet)->required();
    add_vcpkg->add_option("-v,--version", ret->version)->default_str("latest");
    add_vcpkg->add_option("-p,--profiles", ret->profiles)->default_val(std::vector<std::string>{"common"});
    add_vcpkg->add_option("-f,--features", ret->enabled_features);

    return {add_vcpkg, std::move(ret)};
}

Result<void> action(const Parse &parse_args) {
    for (const auto &profile_name : parse_args.profiles) {
        if (auto res = addToProfile(profile_name, parse_args); !res)
            return std::unexpected(res.error());
    }
    return {};
}

} // namespace catalyst::add::vcpkg
