#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

// NOTE: eventually get rid of all calls to profile_composition
auto catalyst::generate::profileComposition(const std::vector<std::string> &p)
    -> std::expected<utils::yaml::Configuration, std::string> {
    catalyst::logger.debug("Composing profiles.");
    try {
        return utils::yaml::Configuration{p};
    } catch (std::exception &err) {
        return std::unexpected(err.what());
    }
}
