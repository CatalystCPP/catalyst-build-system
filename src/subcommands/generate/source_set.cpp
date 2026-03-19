#include <sys/wait.h>

#include <expected>
#include <filesystem>
#include <ranges>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/subcommands/generate.hpp"

namespace fs = std::filesystem;

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
    YAML::Node ignore_config = YAML::LoadFile(ignore_file.string());
    for (const auto &profile : profiles) {
        if (ignore_config[profile]) {
            for (const auto &ignore_pattern : ignore_config[profile]) {
                auto pattern = ignore_pattern.as<std::string>();
                logger.debug("Ignoring pattern: {}", pattern);
                ignore_patterns.insert(pattern);
            }
        }
    }
    return ignore_patterns;
}

std::expected<std::unordered_set<fs::path>, std::string> buildSourceSet(const fs::path &dir,
                                                                        const std::vector<std::string> &profiles) {
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
                if (extension == ".cpp" || extension == ".cxx" || extension == ".cc" || extension == ".c" ||
                    extension == ".cu" || extension == ".cupp") {
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
std::expected<std::unordered_set<fs::path>, std::string> buildSourceSet(const std::vector<std::string> &source_dirs,
                                                                        const std::vector<std::string> &profiles) {
    catalyst::logger.debug("Building source set.");
    namespace sv = std::views;
    auto paths = source_dirs | sv::transform([](const std::string &str) { return fs::path{str}; }) |
                 std::ranges::to<std::vector>();
    std::unordered_set<fs::path> source_set;

    for (const auto &dir : paths) {
        auto source_set_ex = ::buildSourceSet(dir, profiles);
        if (!source_set_ex) {
            return std::unexpected(std::format("Failed to build source set for directory: {}. Error: {}", dir.string(), source_set_ex.error()));
        }
        source_set.insert(source_set_ex->begin(), source_set_ex->end());
    }

    catalyst::logger.debug("Source set built successfully.");
    return source_set;
}
} // namespace catalyst::generate
