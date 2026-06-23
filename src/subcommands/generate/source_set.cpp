#include <sys/wait.h>

#include <expected>
#include <filesystem>
#include <ranges>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace fs = std::filesystem;
using catalyst::Result;

namespace {

std::optional<std::unordered_set<std::string>> createIgnorePatterns(const fs::path &dir,
                                                                    const std::vector<std::string> &profiles) {
    using catalyst::LogLevel, catalyst::logger;

    fs::path ignore_file = dir / ".catalystignore";
    std::unordered_set<std::string> ignore_patterns;

    if (!fs::exists(ignore_file)) {
        logger.debug("No .catalystignore file found in: {}", dir.string());
        return {}; // tihs isn't a failure, just means there are no ignore patterns
                   // to apply since the file doesn't exist
    }
    logger.debug("Found .catalystignore file in: {}", dir.string());
    namespace yaml = catalyst::utils::yaml;
    auto ignore_config = yaml::loadFile(ignore_file);
    if (!ignore_config) {
        logger.warn("Failed to parse {}: {}", ignore_file.string(), ignore_config.error());
        return {};
    }
    for (const auto &profile : profiles) {
        ryml::ConstNodeRef profile_node = yaml::child(ignore_config->crootref(), profile);
        if (!profile_node.readable() || !profile_node.is_seq())
            continue;
        for (ryml::ConstNodeRef ignore_pattern : profile_node.children()) {
            if (auto pattern = yaml::asString(ignore_pattern)) {
                logger.debug("Ignoring pattern: {}", *pattern);
                ignore_patterns.insert(*pattern);
            }
        }
    }
    return ignore_patterns;
}

Result<std::unordered_set<fs::path>> buildSourceSet(const fs::path &dir, const std::vector<std::string> &profiles) {
    using catalyst::LogLevel, catalyst::logger;

    logger.debug("Processing source directory: {}", dir.string());

    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return std::unexpected(std::format("Source directory not found or is not a directory: {}", dir.string()));
    }

    auto ignore_patterns_opt = createIgnorePatterns(dir, profiles);
    std::unordered_set<std::string> ignore_patterns;
    if (ignore_patterns_opt) {
        ignore_patterns = std::move(*ignore_patterns_opt);
    }

    std::unordered_set<fs::path> source_set;

    const std::vector<std::regex> ignore_regexes = [&ignore_patterns]() {
        std::vector<std::regex> ret_vec;
        ret_vec.reserve(ignore_patterns.size());
        for (const auto &ignore_pattern : ignore_patterns) {
            logger.debug("Compiled ignore pattern: {}", ignore_pattern);
            ret_vec.emplace_back(ignore_pattern);
        }
        return ret_vec;
    }();

    for (const auto &entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            bool ignored = false;
            for (const auto &ignore_regex : ignore_regexes) {
                if (std::regex_match(entry.path().filename().string(), ignore_regex)) {
                    logger.debug("Ignoring file: {}", entry.path().string());
                    ignored = true;
                    break;
                }
            }
            if (!ignored) {
                const auto &path = entry.path();
                const std::string extension = path.extension().string();
                if (extension == ".cpp" || extension == ".cxx" || extension == ".cc" || extension == ".c"
                    || extension == ".cu" || extension == ".cupp") {
                    logger.debug("Adding file to source set: {}", path.string());
                    source_set.insert(path);
                }
            }
        }
    }

    return source_set;
}
} // namespace

namespace catalyst::generate {
Result<std::unordered_set<fs::path>> buildSourceSet(const std::vector<std::string> &source_dirs,
                                                    const std::vector<std::string> &profiles) {
    catalyst::logger.debug("Building source set.");
    catalyst::logger.debug("prepended 'common' to source_set profiles");
    std::vector<std::string> common_prepended_profiles = {"common"};
    common_prepended_profiles.insert(common_prepended_profiles.end(), profiles.begin(), profiles.end());
    namespace sv = std::views;
    auto paths = source_dirs | sv::transform([](const std::string &str) { return fs::path{str}; })
                 | std::ranges::to<std::vector>();
    std::unordered_set<fs::path> source_set;

    for (const auto &dir : paths) {
        auto source_set_ex = ::buildSourceSet(dir, common_prepended_profiles);
        if (!source_set_ex) {
            return std::unexpected(std::format(
                "Failed to build source set for directory: {}. Error: {}", dir.string(), source_set_ex.error()));
        }
        source_set.insert(source_set_ex->begin(), source_set_ex->end());
    }

    catalyst::logger.debug("Source set built successfully.");
    return source_set;
}
} // namespace catalyst::generate
