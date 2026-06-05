#include <algorithm>
#include <atomic>
#include <execution>
#include <filesystem>
#include <format>
#include <future>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/fmt.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::fmt {
std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.debug("Fmt subcommand invoked.");
    const std::vector<std::string> &profiles = parse_args.profiles;
    YAML::Node profile_comp;
    catalyst::logger.debug("Composing profiles.");
    auto res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(res.error());
    }
    profile_comp = res.value();

    auto formatter = profile_comp["manifest"]["tooling"]["FMT"].as<std::string>();
    catalyst::logger.debug("Using formatter: {}", formatter);

    namespace fs = std::filesystem;

    std::unordered_set<fs::path> source_dirs;
    std::unordered_set<fs::path> include_dirs;

    for (const auto &node : profile_comp["manifest"]["dirs"]["source"]) {
        source_dirs.insert(fs::absolute(node.as<std::string>()));
    }

    for (const auto &node : profile_comp["manifest"]["dirs"]["include"]) {
        include_dirs.insert(fs::absolute(node.as<std::string>()));
    }

    auto get_ignore_regexes = [](const fs::path &dir, const std::vector<std::string> &profiles) {
        fs::path ignore_file = dir / ".catalystignore";
        std::vector<std::regex> regexes;
        if (fs::exists(ignore_file)) {
            try {
                YAML::Node ignore_config = YAML::LoadFile(ignore_file.string());
                for (const auto &profile : profiles) {
                    if (ignore_config[profile]) {
                        for (const auto &pattern : ignore_config[profile]) {
                            catalyst::logger.debug("Loaded ignore pattern: {} for profile: {} in dir: {}",
                                                   pattern.as<std::string>(),
                                                   profile,
                                                   dir.string());
                            regexes.emplace_back(pattern.as<std::string>());
                        }
                    }
                }
            } catch (...) {
                // Keep moving on configuration parsing errors
            }
        }
        return regexes;
    };

    auto is_ignored = [](const fs::path &path, const std::vector<std::regex> &regexes) {
        return std::ranges::any_of(regexes, [&path](const std::regex &reg) {
            return std::regex_match(path.filename().string(), reg);
        });
    };

    std::vector<std::filesystem::path> files_to_format;
    for (const auto &dir : source_dirs) {
        auto ignore_regexes = get_ignore_regexes(dir, profiles);
        for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                if (is_ignored(entry.path(), ignore_regexes)) {
                    continue;
                }
                if (std::string extension = entry.path().extension(); extension == ".cpp" || extension == ".cxx" ||
                                                                      extension == ".cc" || extension == ".c" ||
                                                                      extension == ".cu" || extension == ".cupp") {
                    files_to_format.push_back(entry.path());
                }
            }
        }
    }

    for (const auto &dir : include_dirs) {
        auto ignore_regexes = get_ignore_regexes(dir, profiles);
        for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                if (is_ignored(entry.path(), ignore_regexes)) {
                    continue;
                }
                if (std::string extension = entry.path().extension(); extension == ".hpp" || extension == ".hxx" ||
                                                                      extension == ".hh" || extension == ".h" ||
                                                                      extension == ".cuh") {
                    files_to_format.push_back(entry.path());
                }
            }
        }
    }

    if (files_to_format.empty()) {
        return {};
    }

    std::vector<std::future<std::expected<void, std::string>>> futures;
    futures.reserve(files_to_format.size());

    for (const auto &file : files_to_format) {
        futures.push_back(
            std::async(std::launch::async, [formatter, file_str = file.string()]() -> std::expected<void, std::string> {
                auto process_res = catalyst::processExec({formatter, "-i", file_str});
                if (!process_res) {
                    return std::unexpected(process_res.error());
                }
                int exit_code = process_res.value().get();
                if (exit_code != 0) {
                    return std::unexpected(
                        std::format("Error running {} on {}. Exit code: {}", formatter, file_str, exit_code));
                }
                return {};
            }));
    }

    bool any_failed = false;
    std::string errors;
    for (auto &f : futures) {
        auto res = f.get();
        if (!res) {
            any_failed = true;
            errors += res.error() + "\n";
        }
    }

    if (any_failed) {
        return std::unexpected(errors);
    }

    catalyst::logger.debug("Fmt subcommand finished successfully.");
    return {};
}
} // namespace catalyst::fmt
