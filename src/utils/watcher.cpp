#include "catalyst/utils/watcher.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <efsw/efsw.hpp>

#include "catalyst/utils/log/log.hpp"

/// After the first event of a burst arrives, wait for this quiet period before rebuilding
/// so editors that emit multiple events per save (atomic rename, metadata touch) trigger once.
static constexpr auto TUNABLE_WATCH_SETTLE_DURATION = std::chrono::milliseconds(200);

namespace catalyst::utils::watcher {

namespace fs = std::filesystem;

Watcher::Watcher(std::vector<fs::path> watch_paths)
    : watch_paths(std::move(watch_paths)), listener(std::make_unique<Listener>(this)),
      file_watcher(std::make_unique<efsw::FileWatcher>()) {
    // efsw only watches directories: watch directory paths recursively, and for single-file
    // paths watch the parent directory (grouped, since efsw rejects duplicate watches) with a
    // filename filter in the listener.
    std::unordered_map<std::string, std::unordered_set<std::string>> file_groups;

    for (const auto &path : this->watch_paths) {
        std::error_code ec;
        if (!fs::exists(path, ec)) {
            catalyst::logger.warn("Watch path does not exist, skipping: {}", path.string());
            continue;
        }
        if (fs::is_directory(path, ec)) {
            efsw::WatchID id = file_watcher->addWatch(path.string(), listener.get(), true);
            if (id < 0) {
                catalyst::logger.warn("Failed to watch {}: {}", path.string(), efsw::Errors::Log::getLastErrorLog());
            }
        } else {
            file_groups[path.parent_path().string()].insert(path.filename().string());
        }
    }

    for (auto &[dir, filenames] : file_groups) {
        efsw::WatchID id = file_watcher->addWatch(dir, listener.get(), false);
        if (id < 0) {
            catalyst::logger.warn("Failed to watch {}: {}", dir, efsw::Errors::Log::getLastErrorLog());
            continue;
        }
        listener->filename_filters[id] = std::move(filenames);
    }

    file_watcher->watch(); // starts the event dispatch thread
}

Watcher::~Watcher() = default;

void Watcher::watch(const std::function<void(const fs::path &)> &on_change) {
    std::unique_lock lock(mutex);
    running = true;
    while (running) {
        cv.wait(lock, [&] { return !running || !pending.empty(); });
        if (!running)
            break;

        // Debounce: wait until no new events arrive within the settle window.
        for (;;) {
            size_t seen = pending.size();
            if (cv.wait_for(lock, TUNABLE_WATCH_SETTLE_DURATION, [&] { return !running || pending.size() != seen; })) {
                if (!running)
                    return;
                continue;
            }
            break;
        }

        fs::path changed = std::move(pending.back());
        pending.clear();

        lock.unlock();
        on_change(changed);
        lock.lock();
    }
}

void Watcher::stop() {
    {
        std::lock_guard lock(mutex);
        running = false;
    }
    cv.notify_all();
}

} // namespace catalyst::utils::watcher
