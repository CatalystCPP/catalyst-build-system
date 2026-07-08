#pragma once
#include <filesystem>
namespace catalyst {
/// @brief RAII guard that changes the current working directory and restores
/// the original directory when destroyed.
///
/// On construction, saves the current working directory and changes to the
/// given path. On destruction, changes back to the saved directory. The guard
/// is non-copyable and non-movable so that exactly one object owns the
/// responsibility of restoring the directory.
class DirectoryChangeGuard {
public:
    /// @brief Saves the current working directory, then changes to @p path_to_change_to.
    /// @param path_to_change_to Directory to change into for the guard's lifetime.
    /// @throws std::filesystem::filesystem_error if the current directory cannot
    /// be read or @p path_to_change_to cannot be changed into.
    explicit DirectoryChangeGuard(const std::filesystem::path &path_to_change_to)
        : original_path(std::filesystem::current_path()) {
        std::filesystem::current_path(path_to_change_to);
    }

    /// @brief Restores the working directory saved at construction.
    /// @note If the original directory cannot be restored, the process is
    /// terminated (via std::terminate). This is deliberate: continuing with an
    /// unknown working directory would make subsequent relative-path
    /// operations wrong or destructive, and there is no safe way to recover.
    ~DirectoryChangeGuard() noexcept {
        std::filesystem::current_path(original_path);
    }

    DirectoryChangeGuard(const DirectoryChangeGuard &) =
        delete; ///< RAII guard must have unique ownership of directory state
    DirectoryChangeGuard &
    operator=(const DirectoryChangeGuard &) = delete; ///< RAII guard must have unique ownership of directory state
    DirectoryChangeGuard(DirectoryChangeGuard &&) =
        delete; ///< moved-from object would restore to invalid path on destruction
    DirectoryChangeGuard &
    operator=(DirectoryChangeGuard &&) = delete; ///< moved-from object would restore to invalid path on destruction

private:
    std::filesystem::path original_path; ///< Working directory to restore on destruction.
};
} // namespace catalyst
