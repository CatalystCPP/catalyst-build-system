#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace catalyst::utils::watcher {

class Watcher {
public:
    explicit Watcher(std::vector<std::filesystem::path> watch_paths);

    /// Starts watching and calls on_change whenever a file in watch_paths (recursively) changes.
    /// This is a blocking call.
    void watch(const std::function<void(const std::filesystem::path&)>& on_change);

    /// Stops the watcher.
    void stop();

private:
    std::vector<std::filesystem::path> watch_paths;
    std::unordered_map<std::string, std::filesystem::file_time_type> file_times;
    bool running = false;

    bool checkChanges(std::filesystem::path& changed_path);
};

} // namespace catalyst::utils::watcher
