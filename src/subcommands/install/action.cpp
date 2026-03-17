#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "catalyst/dir_guard.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/subcommands/install.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::install {
namespace fs = std::filesystem;

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.debug("Install subcommand invoked.");

    // Handle source and target paths
    fs::path source_path = fs::absolute(parse_args.source_path);
    fs::path install_path = fs::absolute(parse_args.target_path);

    if (!fs::exists(source_path) || !fs::is_directory(source_path)) {
        return std::unexpected(std::format("Source directory '{}' does not exist.", source_path.string()));
    }

    catalyst::logger.debug("Changing working directory to: {}", source_path.string());
    catalyst::DirectoryChangeGuard dg(source_path);

    catalyst::logger.debug("Composing profiles.");
    utils::yaml::Configuration config;
    try {
        config = utils::yaml::Configuration(parse_args.profiles);
    } catch (const std::exception &e) {
        return std::unexpected(e.what());
    }

    fs::path build_dir = config.getString("manifest.dirs.build").value_or("build");

    if (!fs::exists(build_dir)) {
        return std::unexpected(
            std::format("Build directory '{}' does not exist in '{}'. Please run 'catalyst build' first.",
                        build_dir.string(),
                        source_path.string()));
    }

    catalyst::logger.info("Installing to: {}", install_path.string());

    try {
        fs::create_directories(install_path);
    } catch (const fs::filesystem_error &e) {
        return std::unexpected(std::format("Failed to create install directory: {}", e.what()));
    }

    std::string type = config.getString("manifest.type").value_or("BINARY");
    std::string target_name = config.getString("manifest.name").value_or("name");
    std::string target_filename;
    std::string import_lib_filename; // For Windows SHAREDLIB
    fs::path artifact_subdir;
    fs::path import_lib_subdir = "lib";

    if (type == "STATICLIB") {
#if defined(_WIN32)
        target_filename = target_name + ".lib";
#else
        target_filename = "lib" + target_name + ".a";
#endif
        artifact_subdir = "lib";
    } else if (type == "SHAREDLIB") {
#if defined(_WIN32)
        target_filename = target_name + ".dll";
        import_lib_filename = target_name + ".lib";
        artifact_subdir = "bin";
#elif defined(__APPLE__)
        target_filename = "lib" + target_name + ".dylib";
        artifact_subdir = "lib";
#else
        target_filename = "lib" + target_name + ".so";
        artifact_subdir = "lib";
#endif
    } else if (type == "BINARY") {
#if defined(_WIN32)
        target_filename = target_name + ".exe";
#else
        target_filename = target_name;
#endif
        artifact_subdir = "bin";
    } else {
        return std::unexpected(
            std::format("Unexpected value for manifest.type: {}. Expected: STATICLIB, SHAREDLIB, or BINARY.", type));
    }

    auto copy_artifact = [&](const fs::path &source,
                             const fs::path &dest_dir,
                             const std::string &filename) -> std::expected<void, std::string> {
        fs::path dest = dest_dir / filename;
        if (fs::exists(source)) {
            catalyst::logger.info("Installing artifact: {} -> {}", source.string(), dest.string());
            try {
                fs::create_directories(dest_dir);
                fs::copy_file(source, dest, fs::copy_options::overwrite_existing);
            } catch (const fs::filesystem_error &e) {
                return std::unexpected(std::format("Failed to install artifact: {}", e.what()));
            }
        } else {
            catalyst::logger.warn("Artifact '{}' not found in build directory. Skipping.", source.string());
        }
        return {};
    };

    fs::path source_artifact = build_dir / target_filename;
    fs::path dest_artifact_dir = install_path / artifact_subdir;

    if (auto res = copy_artifact(source_artifact, dest_artifact_dir, target_filename); !res) {
        return res;
    }

    // Handle Windows import lib for SHAREDLIB
    if (!import_lib_filename.empty()) {
        fs::path source_import_lib = build_dir / import_lib_filename;
        fs::path dest_import_lib_dir = install_path / import_lib_subdir;
        // We don't fail hard if import lib is missing, just warn, but it's usually important.
        // Re-using copy_artifact which warns.
        if (auto res = copy_artifact(source_import_lib, dest_import_lib_dir, import_lib_filename); !res) {
            return res;
        }
    }

    // Install headers if defined
    if (auto include_dirs = config.getStringVector("manifest.dirs.include")) {
        fs::path dest_include_dir = install_path / "include";
        for (const auto &inc_dir : *include_dirs) {
            fs::path source_inc = fs::path(inc_dir); // Relative to current path (which is source_path)
            if (fs::exists(source_inc) && fs::is_directory(source_inc)) {
                catalyst::logger.info("Installing headers from: {} -> {}",
                                     source_inc.string(),
                                     dest_include_dir.string());
                try {
                    fs::create_directories(dest_include_dir);
                    for (const auto &entry : fs::recursive_directory_iterator(source_inc)) {
                        fs::path relative_path = fs::relative(entry.path(), source_inc);
                        fs::path target_path = dest_include_dir / relative_path;
                        if (fs::is_directory(entry)) {
                            fs::create_directories(target_path);
                        } else {
                            fs::create_directories(target_path.parent_path());
                            fs::copy_file(entry.path(), target_path, fs::copy_options::overwrite_existing);
                        }
                    }
                } catch (const fs::filesystem_error &e) {
                    return std::unexpected(std::format("Failed to install headers: {}", e.what()));
                }
            }
        }
    }

    catalyst::logger.debug("Install subcommand finished successfully.");
    return {};
}

} // namespace catalyst::install
