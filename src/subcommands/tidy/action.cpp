#include <atomic>
#include <deque>
#include <expected>
#include <filesystem>
#include <format>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/subcommands/tidy.hpp"

#include "yaml-cpp/node/node.h"

namespace catalyst::tidy {
std::expected<void, std::string> action(const Parse &parse_args) {
    YAML::Node profile_comp;
    auto res = catalyst::generate::profileComposition(parse_args.profiles);
    if (!res)
        return std::unexpected(res.error());
    profile_comp = *res;

    if (!profile_comp["manifest"]["tooling"]["LINTER"] || !profile_comp["manifest"]["tooling"]["LINTER"].IsScalar())
        return std::unexpected("field: manifest.tooling.LINTER is not defined");

    auto linter = profile_comp["manifest"]["tooling"]["LINTER"].as<std::string>();
    // call the linter on the source_set (we can expect clang-tidy like arg syntax) and go on about our day

    catalyst::logger.log(LogLevel::DEBUG, "Building source set.");

    namespace fs = std::filesystem;

    fs::path current_dir = fs::current_path();
    auto relative_source_dirs = profile_comp["manifest"]["dirs"]["source"].as<std::vector<std::string>>();
    std::vector<std::string> absolute_source_dirs;
    absolute_source_dirs.reserve(relative_source_dirs.size());
    for (const auto &dir : relative_source_dirs) {
        absolute_source_dirs.push_back((current_dir / dir).string());
    }

    std::unordered_set<std::filesystem::path> source_set;
    auto source_set_res = generate::buildSourceSet(absolute_source_dirs, parse_args.profiles);
    if (!source_set_res) {
        return std::unexpected(source_set_res.error());
    }
    source_set = *source_set_res;

    unsigned int num_threads = std::thread::hardware_concurrency();
    catalyst::logger.log(
        LogLevel::DEBUG, "Running linter on {} files using {} threads.", source_set.size(), num_threads);

    std::queue<fs::path, std::deque<fs::path>> work_queue(std::deque<fs::path>(source_set.begin(), source_set.end()));
    std::atomic<bool> has_errors = false;
    std::mutex queue_mutex;
    std::vector<std::thread> threads;

    std::mutex err_log_mt;
    threads.reserve(num_threads);
    for (unsigned i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            while (true) {
                fs::path file_to_process;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    if (work_queue.empty()) {
                        break;
                    }
                    file_to_process = work_queue.front();
                    work_queue.pop();
                }

                std::vector<std::string> linter_args{linter};
                if (parse_args.fix) {
                    linter_args.push_back("--fix");
                }
                linter_args.push_back(file_to_process.string());
                if (int res = catalyst::processExec(std::move(linter_args)).value().get(); res != 0) {
                    err_log_mt.lock();
                    catalyst::logger.log(
                        LogLevel::ERROR, "Linter failed for {}: exit code {}", file_to_process.string(), res);
                    has_errors = true;
                    err_log_mt.unlock();
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    if (has_errors) {
        return std::unexpected("Linter finished with errors.");
    }

    catalyst::logger.log(LogLevel::DEBUG, "Tidy subcommand finished successfully.");
    return {};
}
} // namespace catalyst::tidy
