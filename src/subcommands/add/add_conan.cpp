#include <expected>
#include <string>
#include <vector>

#include <catalyst/subcommands/add.hpp>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

using catalyst::Result;

namespace {
Result<void> addToProfile(const std::string &profile, const catalyst::add::conan::Parse &args) {
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
    new_dep["source"] = "conan";
    new_dep["version"] = tree.to_arena(args.version);

    return yaml::profileWriteBack(profile_file);
}
} // namespace

namespace catalyst::add::conan {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &add) {
    CLI::App *add_conan = add.add_subcommand("conan", "add a conan dependency");
    auto ret = std::make_unique<Parse>();

    add_conan->add_option("-n,--name", ret->name, "dependency name")->required();
    add_conan->add_option("-v,--version", ret->version, "dependency version")->required();
    add_conan->add_option("-p,--profiles", ret->profiles, "profiles to add the dependency to")
        ->default_val(std::vector<std::string>{"common"});

    return {add_conan, std::move(ret)};
}

Result<void> action(const Parse &parse_args) {
    for (const auto &profile_name : parse_args.profiles) {
        if (auto res = addToProfile(profile_name, parse_args); !res)
            return std::unexpected(res.error());
    }
    return {};
}

} // namespace catalyst::add::conan
