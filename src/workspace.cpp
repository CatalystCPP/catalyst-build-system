#include "catalyst/workspace.hpp"

#include <format>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst {

namespace fs = std::filesystem;

std::optional<Workspace> Workspace::findRoot(const fs::path &start_path) {
    fs::path current = fs::absolute(start_path);
    fs::path root = current.root_path();

    while (true) {
        fs::path config_path = current / "WORKSPACE.yaml";
        if (fs::exists(config_path)) {
            logger.debug("Found workspace root at: {}", current.string());
            return load(config_path);
        }

        if (current == root) {
            break;
        }
        current = current.parent_path();
    }

    return std::nullopt;
}

std::optional<Workspace> Workspace::load(const fs::path &workspace_file) {
    namespace yaml = utils::yaml;
    auto tree = yaml::loadFile(workspace_file);
    if (!tree) {
        logger.warn("Failed to parse WORKSPACE.yaml: {}. Continuing in non-workspace build.", tree.error());
        return std::nullopt;
    }

    Workspace workspace;
    workspace.root_path = workspace_file.parent_path();

    ryml::ConstNodeRef root = tree->crootref();
    if (!root.is_map()) {
        logger.warn("WORKSPACE.yaml must be a map. Continuing in non-workspace build.");
        return std::nullopt;
    }

    for (ryml::ConstNodeRef member_node : root.children()) {
        if (!member_node.has_key())
            continue;
        std::string key{member_node.key().str, member_node.key().len};

        WorkspaceMember member;
        member.name = key;

        if (auto path = yaml::asString(yaml::child(member_node, "path"))) {
            member.path = workspace.root_path / *path;
        } else {
            member.path = workspace.root_path / key;
        }

        if (ryml::ConstNodeRef profiles = yaml::child(member_node, "profiles"); profiles.readable()) {
            if (auto profile_list = yaml::asStringVector(profiles)) {
                member.profiles = std::move(*profile_list);
            } else {
                logger.warn("Member '{}' profiles is not a sequence, ignoring.", key);
            }
        }

        workspace.members[key] = member;
    }

    return workspace;
}

bool Workspace::contains(const fs::path &path) const {
    fs::path abs_path = fs::absolute(path);
    fs::path rel = abs_path.lexically_relative(root_path);
    return !rel.empty() && rel.native().front() != '.';
}

std::optional<WorkspaceMember> Workspace::getMemberByPath(const fs::path &path) const {
    fs::path abs_path = fs::absolute(path);
    for (const auto &[name, member] : members) {
        if (fs::equivalent(member.path, abs_path)) {
            return member;
        }
    }
    return std::nullopt;
}

std::optional<WorkspaceMember> Workspace::findPackage(std::string_view package_name) const {
    for (const auto &[key, member] : members) {
        try {
            std::vector<std::string> profiles = member.profiles;
            if (profiles.empty())
                profiles.emplace_back("common");

            utils::yaml::Configuration config(profiles, member.path);
            auto name_opt = config.getString("manifest.name");
            if (name_opt && *name_opt == package_name) {
                return member;
            }
        } catch (...) {
            std::ignore;
        }
    }
    return std::nullopt;
}

} // namespace catalyst
