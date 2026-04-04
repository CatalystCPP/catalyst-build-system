#include <sys/wait.h>

#include <exception>
#include <expected>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/node/node.h"

namespace catalyst::generate {
// NOTE: eventually get rid of all calls to profile_composition
std::expected<YAML::Node, std::string> profileComposition(const std::vector<std::string> &p) {
    catalyst::logger.debug("Composing profiles.");
    catalyst::logger.debug("Profile composition finished.");
    try {
        return YAML::Clone(utils::yaml::Configuration{p}.getRoot());
    } catch (std::exception &err) {
        return std::unexpected(err.what());
    }
}
} // namespace catalyst::generate
