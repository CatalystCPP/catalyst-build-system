#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace catalyst {

/// @brief An individual member package of a catalyst workspace
struct WorkspaceMember {
    std::string name;                  ///< name of the package
    std::filesystem::path path;        ///< path to the package
    std::vector<std::string> profiles; ///< profiles used to build the package
};

/// @brief Utility class for all members of a catalyst workspace
class Workspace {
private:
    /// @brief utility to turn WORKSPACE.yaml into a catalyst::Workspace
    static std::optional<Workspace> load(const std::filesystem::path &workspace_file);
public:
    /// @brief utility to find WORKSPACE.yaml and return a catalyst::Workspace
    /// Recurses up until WORKSPACE.yaml is found or we go to root directory
    static std::optional<Workspace> findRoot(const std::filesystem::path &start_path = std::filesystem::current_path());

    /// @brief return directory containing WORKSPACE.yaml
    const std::filesystem::path &getRoot() const {
        return root_path;
    }
    const std::unordered_map<std::string, WorkspaceMember> &getMembers() const {
        return members;
    }

    /// @brief Check if a path is within the workspace
    bool contains(const std::filesystem::path &path) const;

    /// @brief Get member by path (if it is a registered member)
    std::optional<WorkspaceMember> getMemberByPath(const std::filesystem::path &path) const;

    /// @brief Find member by package name (manifest.name)
    // Note: This involves loading configuration of members
    std::optional<WorkspaceMember> findPackage(std::string_view package_name) const;

private:
    std::filesystem::path root_path;                            ///< path containing WORKSPACE.yaml
    /// @brief map of workspace members
    /// ryaml::ConstNodeRef::key -> WorkspaceMemebr
    std::unordered_map<std::string, WorkspaceMember> members;
};

} // namespace catalyst
