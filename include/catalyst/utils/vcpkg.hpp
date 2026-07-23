#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "catalyst/utils/result.hpp"

namespace catalyst::utils::vcpkg {

struct Dependency {
    std::string name;
    std::string version;
    std::string triplet;
    std::vector<std::string> features;
    std::optional<int> port_version;
};

struct InstalledPackage {
    std::string version;
    int port_version = 0;
};

struct InstallResult {
    std::string builtin_baseline;
    std::filesystem::path install_root;
    std::unordered_map<std::string, InstalledPackage> packages;
};

[[nodiscard]] std::string statusKey(std::string_view name, std::string_view triplet);

[[nodiscard]] Result<std::string> currentBuiltinBaseline(const std::filesystem::path &vcpkg_root);

[[nodiscard]] Result<std::string> manifestJson(std::span<const Dependency> dependencies,
                                               std::string_view builtin_baseline);

[[nodiscard]] std::unordered_map<std::string, InstalledPackage> parseInstalledVersions(std::string_view status);

[[nodiscard]] Result<InstallResult> install(std::span<const Dependency> dependencies,
                                            const std::filesystem::path &build_dir,
                                            const std::optional<std::string> &builtin_baseline = std::nullopt);

} // namespace catalyst::utils::vcpkg
