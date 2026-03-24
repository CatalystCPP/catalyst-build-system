#include "catalyst/utils/watcher.hpp"
#include "catalyst/utils/log/log.hpp"
#include <chrono>
#include <filesystem>
#include <thread>

namespace catalyst::utils::watcher {

namespace fs = std::filesystem;

Watcher::Watcher(std::vector<fs::path> watch_paths)
    : watch_paths(std::move(watch_paths)) {
    // Initial scan to populate file_times_
    std::filesystem::path dummy;
    checkChanges(dummy);
}

void Watcher::watch(const std::function<void(const fs::path&)>& on_change) {
    running = true;
    while (running) {
        fs::path changed_path;
        if (checkChanges(changed_path)) {
            on_change(changed_path);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void Watcher::stop() {
    running = false;
}

bool Watcher::checkChanges(fs::path& changed_path) {
    bool changed = false;
    std::unordered_map<std::string, fs::file_time_type> current_times;

    for (const auto& root : watch_paths) {
        if (!fs::exists(root)) continue;

        if (fs::is_regular_file(root)) {
            try {
                auto time = fs::last_write_time(root);
                current_times[root.string()] = time;
                if (!file_times.contains(root.string()) || file_times[root.string()] != time) {
                    if (!file_times.empty()) { // Don't trigger on initial scan
                        changed = true;
                        changed_path = root;
                    }
                }
            } catch (...) {}
            continue;
        }

        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            const auto& path = entry.path();
            try {
                auto time = fs::last_write_time(path);
                current_times[path.string()] = time;

                if (!file_times.contains(path.string()) || file_times[path.string()] != time) {
                    if (!file_times.empty()) {
                        changed = true;
                        changed_path = path;
                        // We could break here if we only want one change at a time,
                        // but we need to update all times to avoid repeated triggers
                    }
                }
            } catch (...) {
                // Ignore files that might be deleted during scan
            }
        }
    }

    // Check for deleted files
    if (!file_times.empty()) {
        for (const auto& [path, time] : file_times) {
            if (!current_times.contains(path)) {
                changed = true;
                changed_path = path;
                break;
            }
        }
    }

    file_times = std::move(current_times);
    return changed;
}

} // namespace catalyst::utils::watcher
