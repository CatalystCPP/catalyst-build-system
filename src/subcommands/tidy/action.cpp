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

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/subcommands/tidy.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"

namespace catalyst::tidy {
Result<void> action(const Parse &parse_args) {
    auto res = catalyst::generate::profileComposition(parse_args.profiles);
    if (!res)
        return std::unexpected(res.error());
    const utils::yaml::Configuration &profile_comp = *res;

    auto linter_opt = profile_comp.getString("manifest.tooling.LINTER");
    if (!linter_opt)
        return std::unexpected("field: manifest.tooling.LINTER is not defined");

    const std::string &linter = *linter_opt;
    // call the linter on the source_set (we can expect clang-tidy like arg syntax) and go on about our day

    catalyst::logger.debug("Building source set.");

    namespace fs = std::filesystem;

    fs::path current_dir = fs::current_path();
    auto source_dirs_opt = profile_comp.getStringVector("manifest.dirs.source");
    if (!source_dirs_opt)
        return std::unexpected("Unable to get value for manifest.dirs.source");
    const std::vector<std::string> &relative_source_dirs = *source_dirs_opt;
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
    catalyst::logger.debug("Running linter on {} files using {} threads.", source_set.size(), num_threads);

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
                    std::lock_guard<std::mutex> lock{err_log_mt};
                    catalyst::logger.error("Linter failed for {}: exit code {}", file_to_process.string(), res);
                    has_errors = true;
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

    catalyst::logger.debug("Tidy subcommand finished successfully.");
    return {};
}
} // namespace catalyst::tidy
