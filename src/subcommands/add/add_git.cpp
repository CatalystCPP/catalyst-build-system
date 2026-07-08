#include <expected>
#include <string>

#include <catalyst/subcommands/add.hpp>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

using catalyst::Result;

namespace {
Result<void> addToProfile(const std::string &profile, const catalyst::add::git::Parse &args) {
    namespace yaml = catalyst::utils::yaml;
    auto res = yaml::loadProfileFile(profile);
    if (!res) {
        return std::unexpected(res.error());
    }
    yaml::ProfileFile &profile_file = res.value();
    ryml::Tree &tree = profile_file.tree;

    ryml::NodeRef dependencies = yaml::childOrCreate(profile_file.profile(), "dependencies");
    dependencies |= ryml::SEQ;

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
    for (ryml::ConstNodeRef dep : dependencies.children()) {
        if (yaml::asString(yaml::child(dep, "name")) == name) {
            dependency_found = true;
            break;
        }
    }

    if (dependency_found) {
        return std::unexpected("Dependency '" + name + "' already exists in profile '" + profile + "'.");
    }

    ryml::NodeRef new_dep = dependencies.append_child();
    new_dep |= ryml::MAP;
    new_dep["name"] = tree.to_arena(name);
    new_dep["source"] = "git";
    new_dep["url"] = tree.to_arena(args.remote);
    new_dep["version"] = tree.to_arena(args.version);
    if (!args.enabled_features.empty()) {
        ryml::NodeRef using_node = yaml::childOrCreate(new_dep, "using");
        using_node |= ryml::SEQ;
        for (const auto &feature : args.enabled_features)
            using_node.append_child() = tree.to_arena(feature);
    }

    return yaml::profileWriteBack(profile_file);
}
} // namespace

namespace catalyst::add::git {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &add) {
    CLI::App *add_git = add.add_subcommand("git", "add a remote git dependency");
    auto ret = std::make_unique<Parse>();

    add_git->add_option("remote", ret->remote, "remote git repository URL");
    add_git->add_option("-n,--name", ret->name, "dependency name");
    add_git->add_option("-v,--version", ret->version, "dependency version")->default_val("latest");
    add_git->add_option("-f,--features", ret->enabled_features, "features to enable");
    add_git->add_option("-p,--profiles", ret->profiles, "profiles to add the dependency to")
        ->default_val(std::vector<std::string>{"common"});

    return {add_git, std::move(ret)};
}

Result<void> action(const Parse &parse_args) {
    for (const auto &profile_name : parse_args.profiles) {
        if (auto res = addToProfile(profile_name, parse_args); !res)
            return std::unexpected(res.error());
    }
    return {};
}

}; // namespace catalyst::add::git
