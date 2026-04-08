#include <expected>
#include <filesystem>
#include <fstream>

#include <catalyst/utils/yaml/profile_write_back.hpp>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/yaml.h>

#include "catalyst/utils/log/log.hpp"

namespace catalyst::utils::yaml {
std::expected<void, std::string> profileWriteBack(const ProfileFile &profile_file) {
    catalyst::logger.debug("Writing profile file: {}", profile_file.path.string());

    std::ofstream profile_file_out{profile_file.path};
    YAML::Emitter emmiter;
    emmiter << profile_file.root_node;
    // NOLINTBEGIN(performance-avoid-endl)
    profile_file_out << emmiter.c_str() << std::endl;
    // NOLINTEND(performance-avoid-endl)
    catalyst::logger.debug("Profile file written successfully.");
    return {};
}
} // namespace catalyst::utils::yaml
