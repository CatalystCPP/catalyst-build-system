#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace catalyst {

struct WorkspaceMember {
    std::string name;
    std::filesystem::path path;
    std::vector<std::string> profiles;
};

class Workspace {
public:
    static std::optional<Workspace> findRoot(const std::filesystem::path &start_path = std::filesystem::current_path());
    static std::optional<Workspace> load(const std::filesystem::path &workspace_file);

    const std::filesystem::path &getRoot() const {
        return root_path;
    }
    const std::unordered_map<std::string, WorkspaceMember> &getMembers() const {
        return members;
    }

    // Check if a path is within the workspace
    bool contains(const std::filesystem::path &path) const;

    // Get member by path (if it is a registered member)
    std::optional<WorkspaceMember> getMemberByPath(const std::filesystem::path &path) const;

    // Find member by package name (manifest.name)
    // Note: This involves loading configuration of members
    std::optional<WorkspaceMember> findPackage(std::string_view package_name) const;

private:
    std::filesystem::path root_path;
    std::unordered_map<std::string, WorkspaceMember> members;
};

} // namespace catalyst
