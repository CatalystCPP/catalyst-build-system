#include <expected>
#include <string>

#include <catalyst/subcommands/add.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

using catalyst::Result;

namespace {

Result<void> addToProfile(const std::string &profile, const catalyst::add::local::Parse &args) {
    namespace yaml = catalyst::utils::yaml;
    auto profile_file = yaml::loadProfileFile(profile);
    if (!profile_file)
        return std::unexpected(profile_file.error());
    ryml::Tree &tree = profile_file->tree;

    ryml::NodeRef deps = yaml::childOrCreate(profile_file->profile(), "dependencies");
    deps |= ryml::SEQ;
    bool exists = false;
    for (ryml::ConstNodeRef d : deps.children()) {
        if (yaml::asString(yaml::child(d, "name")) == args.name) {
            exists = true;
            break;
        }
    }
    if (exists)
        return std::unexpected("Dependency '" + args.name + "' already exists in profile '" + profile + "'.");

    ryml::NodeRef new_dep = deps.append_child();
    new_dep |= ryml::MAP;
    new_dep["name"] = tree.to_arena(args.name);
    new_dep["source"] = "local";
    new_dep["path"] = tree.to_arena(args.path);
    if (!args.enabled_features.empty()) {
        ryml::NodeRef using_node = yaml::childOrCreate(new_dep, "using");
        using_node |= ryml::SEQ;
        for (const auto &feature : args.enabled_features)
            using_node.append_child() = tree.to_arena(feature);
    }

    return yaml::profileWriteBack(*profile_file);
}

} // namespace

namespace catalyst::add::local {

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    auto *cmd = app.add_subcommand("local", "add a local dependency");
    auto ret = std::make_unique<Parse>();
    cmd->add_option("-n,--name", ret->name, "dependency name")->required();
    cmd->add_option("path", ret->path, "local package directory path")->required();
    cmd->add_option("-p,--profiles", ret->profiles, "profiles to add the dependency to")
        ->default_val(std::vector<std::string>{"common"});
    cmd->add_option("-f,--features", ret->enabled_features, "features to enable");
    return {cmd, std::move(ret)};
}

Result<void> action(const Parse &args) {
    for (const auto &profile : args.profiles)
        if (auto res = addToProfile(profile, args); !res)
            return std::unexpected(res.error());
    return {};
}

} // namespace catalyst::add::local
