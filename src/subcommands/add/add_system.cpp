#include <expected>
#include <string>

#include <catalyst/subcommands/add.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

using catalyst::Result;

namespace {
Result<void> addToProfile(const std::string &profile, const catalyst::add::system::Parse &args) {
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
    new_dep["source"] = "system";
    if (!args.lib_path.empty())
        new_dep["lib"] = tree.to_arena(args.lib_path);
    if (!args.inc_path.empty())
        new_dep["include"] = tree.to_arena(args.inc_path);

    return yaml::profileWriteBack(profile_file);
}
} // namespace

namespace catalyst::add::system {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *add_system = app.add_subcommand("system", "add a system dependency");
    auto ret = std::make_unique<Parse>();

    add_system->add_option("-n,--name", ret->name)->required();
    add_system->add_option("-l,--lib", ret->lib_path);
    add_system->add_option("-i,--inc", ret->inc_path);
    add_system->add_option("-p,--profiles", ret->profiles);

    return {add_system, std::move(ret)};
}

Result<void> action(const Parse &parse_args) {
    for (const auto &profile_name : parse_args.profiles) {
        if (auto res = addToProfile(profile_name, parse_args); !res)
            return std::unexpected(res.error());
    }
    return {};
}

}; // namespace catalyst::add::system
