#include <expected>
#include <filesystem>
#include <fstream>

#include <catalyst/utils/yaml/profile_write_back.hpp>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/yaml.h>

#include "catalyst/utils/log/log.hpp"

namespace catalyst::utils::yaml {
std::expected<void, std::string> profileWriteBack(const std::string &profile_name, const YAML::Node &node) {
    catalyst::logger.debug("Writing profile to file: {}", profile_name);
    namespace fs = std::filesystem;
    fs::path profile_path;
    if (profile_name == "common")
        profile_path = "catalyst.yaml";
    else
        profile_path = std::format("catalyst_{}.yaml", profile_name);

    catalyst::logger.debug("Profile path: {}", profile_path.string());
    if (!fs::exists(profile_path)) {
        catalyst::logger.debug("Creating new profile file: {}", profile_path.string());
    }

    std::ofstream profile_file{profile_path};
    YAML::Emitter emmiter;
    emmiter << node;
    // NOLINTBEGIN(performance-avoid-endl)
    profile_file << emmiter.c_str() << std::endl;
    // NOLINTEND(performance-avoid-endl)
    catalyst::logger.debug("Profile file written successfully.");
    return {};
}
} // namespace catalyst::utils::yaml
