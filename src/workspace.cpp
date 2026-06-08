#include "catalyst/workspace.hpp"

#include <format>

#include <yaml-cpp/yaml.h>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

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
    try {
        YAML::Node node = YAML::LoadFile(workspace_file.string());
        Workspace workspace;
        workspace.root_path = workspace_file.parent_path();

        if (!node.IsMap()) {
            logger.warn("WORKSPACE.yaml must be a map. Continuing in non-workspace build.");
            return std::nullopt;
        }

        for (const auto &kv : node) {
            auto key = kv.first.as<std::string>();
            YAML::Node value = kv.second;

            WorkspaceMember member;
            member.name = key;

            if (value["path"]) {
                member.path = workspace.root_path / value["path"].as<std::string>();
            } else {
                member.path = workspace.root_path / key;
            }

            if (value["profiles"]) {
                if (value["profiles"].IsSequence()) {
                    member.profiles = value["profiles"].as<std::vector<std::string>>();
                } else {
                    logger.warn("Member '{}' profiles is not a sequence, ignoring.", key);
                }
            }

            workspace.members[key] = member;
        }

        return workspace;

    } catch (const YAML::Exception &e) {
        logger.warn("Failed to parse WORKSPACE.yaml: {}. Continuing in non-workspace build.", e.what());
        return std::nullopt;
    }
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
