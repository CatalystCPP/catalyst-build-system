#pragma once
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::utils::yaml {

inline std::filesystem::path multiplexedBuildDir(const std::string &base, const std::vector<std::string> &profiles) {
    std::string suffix;
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        if (i != 0)
            suffix += '-';
        suffix += profiles[i];
    }
    if (suffix.empty())
        return std::filesystem::path{base};
    return std::filesystem::path{base} / suffix;
}

class Configuration {
public:
    Configuration();
    explicit Configuration(const std::vector<std::string> &profiles,
                           const std::filesystem::path &root_dir = std::filesystem::current_path());
    Configuration(const Configuration &) = delete;
    Configuration(Configuration &&) noexcept;
    Configuration &operator=(const Configuration &) = delete;
    Configuration &operator=(Configuration &&) noexcept;
    ~Configuration();

    [[nodiscard]] bool has(const std::string &key) const;

    [[nodiscard]] std::optional<std::string> getString(const std::string &key) const;
    [[nodiscard]] std::optional<int> getInt(const std::string &key) const;
    [[nodiscard]] std::optional<bool> getBool(const std::string &key) const;
    [[nodiscard]] std::optional<std::vector<std::string>> getStringVector(const std::string &key) const;

    [[nodiscard]] std::filesystem::path getBuildDir() const;

    /** Synchronizes the read-only hook snapshot with the active lifecycle phase. */
    [[nodiscard]] Result<void> syncHookState(std::string_view hook_name) const;

    /** Returns the environment inherited by command, script, and codegen hooks. */
    [[nodiscard]] std::unordered_map<std::string, std::string> hookEnvironment(std::string_view hook_name) const;

    /** The composed configuration tree (rapidyaml). */
    [[nodiscard]] ryml::ConstNodeRef rootRef() const {
        return composition.crootref();
    }

private:
    struct SnapshotFile;

    ryml::Tree composition;
    std::vector<std::string> profile_names;
    std::filesystem::path root_dir;
    mutable std::unique_ptr<SnapshotFile> snapshot_file;
};
} // namespace catalyst::utils::yaml
