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

namespace {
std::expected<void, std::string> execBatch(std::vector<std::basic_string<char>> args);
}

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

    if (files_to_format.empty()) {
        return {};
    }

    constexpr size_t MAX_ARG_BYTES = 1 << (10 + 7); // 128 KiB limit
    size_t current_bytes = formatter.size() + 3;    // formatter + " -i"
    std::vector<std::string> args = {formatter, "-i"};

    for (const fs::path &file_to_format : files_to_format) {
        std::string file_str = file_to_format.string();
        size_t entry_size = file_str.size() + 1; // +1 for separator/null

        if (args.size() > 2 && current_bytes + entry_size > MAX_ARG_BYTES) {
            if (auto res = execBatch(std::move(args)); !res)
                return res;
            args = {formatter, "-i"};
            current_bytes = formatter.size() + 3;
        }

        args.push_back(std::move(file_str));
        current_bytes += entry_size;
    }

    if (args.size() > 2)
        if (auto res = execBatch(std::move(args)); !res)
            return res;

    catalyst::logger.debug("Fmt subcommand finished successfully.");
    return {};
}
} // namespace catalyst::fmt

namespace {
std::expected<void, std::string> execBatch(std::vector<std::basic_string<char>> args) {
    if (int res = catalyst::processExec(std::forward<decltype(args)>(args)).value().get(); res) {
        std::string error_message = "Error running clang-format. Exit code: " + std::to_string(res);
        catalyst::logger.error("{}", error_message);
        return std::unexpected(error_message);
    }
    return {};
}
} // namespace
