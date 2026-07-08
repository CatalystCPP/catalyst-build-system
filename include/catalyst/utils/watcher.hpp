#pragma once
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

#include <efsw/efsw.hpp>

namespace catalyst::utils::watcher {

class Watcher {
public:
    explicit Watcher(std::vector<std::filesystem::path> watch_paths);
    ~Watcher();

    Watcher(const Watcher &) = delete;
    Watcher(Watcher &&) = delete;
    Watcher &operator=(const Watcher &) = delete;
    Watcher &operator=(Watcher &&) = delete;

    /// Starts watching and calls on_change whenever a file in watch_paths (recursively) changes.
    /// This is a blocking call.
    void watch(const std::function<void(const std::filesystem::path &)> &on_change);

    /// Stops the watcher.
    void stop();

private:
    struct Listener : efsw::FileWatchListener {
        Watcher *owner;
        /// Watches created for single-file watch paths only accept events for those filenames.
        std::unordered_map<efsw::WatchID, std::unordered_set<std::string>> filename_filters;

        explicit Listener(Watcher *owner) : owner(owner) {
        }

        void handleFileAction(efsw::WatchID watchid,
                              const std::string &dir,
                              const std::string &filename,
                              efsw::Action /*action*/,
                              std::string /*old_filename*/) override {

            if (auto it = filename_filters.find(watchid);
                it != filename_filters.end() && !it->second.contains(filename)) {
                return;
            }

            std::filesystem::path changed = std::filesystem::path(dir) / filename;
            std::error_code ec;
            if (std::filesystem::is_directory(changed, ec)) {
                return; // directory creation/modification itself is not a rebuild trigger
            }

            {
                std::lock_guard lock(owner->mutex);
                owner->pending.push_back(std::move(changed));
            }
            owner->cv.notify_one();
        }
    };

    std::vector<std::filesystem::path> watch_paths;
    std::unique_ptr<Listener> listener;
    std::unique_ptr<efsw::FileWatcher> file_watcher;

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::filesystem::path> pending;
    bool running = false;
};

} // namespace catalyst::utils::watcher
