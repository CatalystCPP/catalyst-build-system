#include <expected>
#include <filesystem>
#include <format>
#include <string>

#include <catalyst/utils/yaml/load_profile_file.hpp>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/yaml.h>

#include "catalyst/utils/log/log.hpp"

namespace catalyst::utils::yaml {

namespace {

namespace fs = std::filesystem;

auto loadFromCombined(const std::string &profile, const fs::path &combined_path, const fs::path &profile_path)
    -> std::expected<ProfileFile, std::string> {
    try {
        YAML::Node ret = YAML::LoadFile(combined_path);

        if (fs::exists(profile_path)) {
            catalyst::logger.warn("Profile: {} was moved into CATALYST.yaml", profile);
            YAML::Node profile_node = YAML::LoadFile(profile_path);
            ret[profile] = profile_node;
            // DON'T DELETE THE FILE, just ignore it. This way we can preserve user changes and avoid data loss.
        } else if (!ret[profile]) {
            ret[profile] = YAML::Node(YAML::NodeType::Map);
        }

        catalyst::logger.debug("Combined profile file loaded successfully.");
        return ProfileFile{.root_node = ret, .profile_node = ret[profile], .path = combined_path};
    } catch (YAML::Exception &err) {
        return std::unexpected(std::format("Failed to parse YAML file: {}", err.what()));
    }
}

auto loadFromIsolate(const std::string &profile, const fs::path &profile_path)
    -> std::expected<ProfileFile, std::string> {
    if (!fs::exists(profile_path))
        return std::unexpected(std::format("Profile file: {} for {} not found", profile_path.string(), profile));

    try {
        YAML::Node ret = YAML::LoadFile(profile_path);
        catalyst::logger.debug("Profile file loaded successfully.");
        return ProfileFile{.root_node = ret, .profile_node = ret, .path = profile_path};
    } catch (YAML::Exception &err) {
        return std::unexpected(std::format("Failed to parse YAML file: {}", err.what()));
    }
}

} // anonymous namespace

auto loadProfileFile(const std::string &profile, const fs::path &root_dir) -> std::expected<ProfileFile, std::string> {
    catalyst::logger.debug("Loading profile file: {} from {}", profile, root_dir.string());

    const fs::path combined_path = root_dir / "CATALYST.yaml";
    fs::path profile_path = root_dir;
    if (profile == "common")
        profile_path /= "catalyst.yaml";
    else
        profile_path /= std::format("catalyst_{}.yaml", profile);

    catalyst::logger.debug("Profile path: {}", profile_path.string());

    if (fs::exists(combined_path))
        return loadFromCombined(profile, combined_path, profile_path);

    return loadFromIsolate(profile, profile_path);
}

} // namespace catalyst::utils::yaml
