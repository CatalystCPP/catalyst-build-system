#include <algorithm>
#include <atomic>
#include <execution>
#include <filesystem>
#include <format>
#include <mutex>
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
    catalyst::logger.log(LogLevel::DEBUG, "Fmt subcommand invoked.");
    const std::vector<std::string> &profiles = parse_args.profiles;
    YAML::Node profile_comp;
    catalyst::logger.log(LogLevel::DEBUG, "Composing profiles.");
    auto res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(res.error());
    }
    profile_comp = res.value();

    auto formatter = profile_comp["manifest"]["tooling"]["FMT"].as<std::string>();
    catalyst::logger.log(LogLevel::DEBUG, "Using formatter: {}", formatter);

    namespace fs = std::filesystem;

    std::unordered_set<fs::path> source_dirs;
    std::unordered_set<fs::path> include_dirs;

    for (const auto &node : profile_comp["manifest"]["dirs"]["source"]) {
        source_dirs.insert(node.as<std::string>());
    }

    for (const auto &node : profile_comp["manifest"]["dirs"]["include"]) {
        include_dirs.insert(node.as<std::string>());
    }

    std::vector<std::filesystem::path> files_to_format;
    for (const auto &dir : source_dirs) {
        for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                if (std::string extension = entry.path().extension();
                    extension == ".cc" || extension == ".cpp" || extension == ".c") {
                    files_to_format.push_back(entry.path());
                }
            }
        }
    }

    for (const auto &dir : include_dirs) {
        for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                if (std::string extension = entry.path().extension(); extension == ".hpp" || extension == ".h") {
                    files_to_format.push_back(entry.path());
                }
            }
        }
    }

    std::atomic<bool> formatting_error = false;
    std::string error_message;
    std::mutex error_mutex;

    std::for_each(std::execution::par, files_to_format.begin(), files_to_format.end(), [&](const auto &file) -> void {
        if (formatting_error) {
            return;
        }
        catalyst::logger.log(LogLevel::DEBUG, "Formatting {}", file.string());
        if (int res = catalyst::processExec({formatter, "-i", file.string()}).value().get(); res) {
            std::lock_guard<std::mutex> lock(error_mutex);
            if (!formatting_error) {
                error_message = "Error running clang-format on " + file.string();
                catalyst::logger.log(LogLevel::ERROR, "{}", error_message);
                formatting_error = true;
            }
        }
    });

    if (formatting_error) {
        return std::unexpected(error_message);
    }

    catalyst::logger.log(LogLevel::DEBUG, "Fmt subcommand finished successfully.");
    return {};
}
} // namespace catalyst::fmt
