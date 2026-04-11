#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

// NOTE: eventually get rid of all calls to profile_composition
auto catalyst::generate::profileComposition(const std::vector<std::string> &p)
    -> std::expected<YAML::Node, std::string> {
    catalyst::logger.debug("Composing profiles.");
    catalyst::logger.debug("Profile composition finished.");
    try {
        return YAML::Clone(utils::yaml::Configuration{p}.getRoot());
    } catch (std::exception &err) {
        return std::unexpected(err.what());
    }
}
