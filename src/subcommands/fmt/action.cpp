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

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/fmt.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::fmt {
Result<void> action(const Parse &parse_args) {
    catalyst::logger.debug("Fmt subcommand invoked.");
    const std::vector<std::string> &profiles = parse_args.profiles;
    catalyst::logger.debug("Composing profiles.");
    auto res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(res.error());
    }
    const utils::yaml::Configuration &profile_comp = res.value();

    auto formatter_opt = profile_comp.getString("manifest.tooling.FMT");
    if (!formatter_opt) {
        return std::unexpected("field: manifest.tooling.FMT is not defined");
    }
    const std::string &formatter = *formatter_opt;
    catalyst::logger.debug("Using formatter: {}", formatter);

    namespace fs = std::filesystem;

    std::unordered_set<fs::path> source_dirs;
    std::unordered_set<fs::path> include_dirs;

    for (const auto &dir : profile_comp.getStringVector("manifest.dirs.source").value_or(std::vector<std::string>{})) {
        source_dirs.insert(fs::absolute(dir));
    }

    for (const auto &dir : profile_comp.getStringVector("manifest.dirs.include").value_or(std::vector<std::string>{})) {
        include_dirs.insert(fs::absolute(dir));
    }

    auto get_ignore_regexes = [](const fs::path &dir, const std::vector<std::string> &profiles) {
        namespace yaml = catalyst::utils::yaml;
        fs::path ignore_file = dir / ".catalystignore";
        std::vector<std::regex> regexes;
        if (fs::exists(ignore_file)) {
            try {
                auto ignore_config = yaml::loadFile(ignore_file);
                if (!ignore_config)
                    return regexes; // Keep moving on configuration parsing errors
                for (const auto &profile : profiles) {
                    ryml::ConstNodeRef profile_node = yaml::child(ignore_config->crootref(), profile);
                    if (!profile_node.readable() || !profile_node.is_seq())
                        continue;
                    for (ryml::ConstNodeRef pattern_node : profile_node.children()) {
                        if (auto pattern = yaml::asString(pattern_node)) {
                            catalyst::logger.debug("Loaded ignore pattern: {} for profile: {} in dir: {}",
                                                   *pattern,
                                                   profile,
                                                   dir.string());
                            regexes.emplace_back(*pattern);
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
        return std::ranges::any_of(
            regexes, [&path](const std::regex &reg) { return std::regex_match(path.filename().string(), reg); });
    };

    std::vector<std::string> common_prepended_profiles = {"common"};
    common_prepended_profiles.insert(common_prepended_profiles.end(), profiles.begin(), profiles.end());

    std::vector<std::filesystem::path> files_to_format;
    for (const auto &dir : source_dirs) {
        auto ignore_regexes = get_ignore_regexes(dir, common_prepended_profiles);
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
        auto ignore_regexes = get_ignore_regexes(dir, common_prepended_profiles);
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

    std::vector<std::future<Result<void>>> futures;
    futures.reserve(files_to_format.size());

    for (const auto &file : files_to_format) {
        futures.push_back(std::async(std::launch::async, [formatter, file_str = file.string()]() -> Result<void> {
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
