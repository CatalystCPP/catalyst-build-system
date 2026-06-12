#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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
    Configuration() = default;
    explicit Configuration(const std::vector<std::string> &profiles,
                           const std::filesystem::path &root_dir = std::filesystem::current_path());

    bool has(const std::string &key) const;

    std::optional<std::string> getString(const std::string &key) const;
    std::optional<int> getInt(const std::string &key) const;
    std::optional<bool> getBool(const std::string &key) const;
    std::optional<std::vector<std::string>> getStringVector(const std::string &key) const;

    std::filesystem::path getBuildDir() const;

    /** The composed configuration tree (rapidyaml). */
    ryml::ConstNodeRef rootRef() const {
        return composition.crootref();
    }

private:
    ryml::Tree composition;
    std::vector<std::string> profile_names;
};
} // namespace catalyst::utils::yaml
