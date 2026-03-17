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
std::expected<YAML::Node, std::string> loadProfileFile(const std::string &profile,
                                                       const std::filesystem::path &root_dir) {
    catalyst::logger.debug("Loading profile file: {} from {}", profile, root_dir.string());
    namespace fs = std::filesystem;
    fs::path profile_path = root_dir;
    if (profile == "common")
        profile_path /= "catalyst.yaml";
    else
        profile_path /= std::format("catalyst_{}.yaml", profile);

    catalyst::logger.debug("Profile path: {}", profile_path.string());
    if (!fs::exists(profile_path)) {
        return std::unexpected(std::format("Profile file: {} for {} not found", profile_path.string(), profile));
    }

    try {
        YAML::Node ret = YAML::LoadFile(profile_path);
        catalyst::logger.debug("Profile file loaded successfully.");
        return ret;
    } catch (YAML::Exception &err) {
        return std::unexpected(std::format("Failed to parse YAML file: {}", err.what()));
    }
}
} // namespace catalyst::utils::yaml
