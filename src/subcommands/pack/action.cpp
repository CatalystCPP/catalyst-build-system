#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "catalyst/dir_guard.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/install.hpp"
#include "catalyst/subcommands/pack.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::pack {
namespace fs = std::filesystem;

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.debug("Pack subcommand invoked.");

    fs::path source_path = fs::absolute(parse_args.source_path);
    fs::path target_path = fs::absolute(parse_args.target_path);

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

    fs::path build_dir = config.getBuildDir();
    if (!fs::exists(build_dir)) {
        return std::unexpected(
            std::format("Build directory '{}' does not exist in '{}'. Please run 'catalyst build' first.",
                        build_dir.string(),
                        source_path.string()));
    }

    // 1. Stage the installation files
    fs::path staging_dir = build_dir / "pack_stage";
    if (fs::exists(staging_dir)) {
        fs::remove_all(staging_dir);
    }

    catalyst::install::Parse install_args;
    install_args.profiles = parse_args.profiles;
    install_args.source_path = source_path;
    install_args.target_path = staging_dir;

    catalyst::logger.debug("Staging artifacts to: {}", staging_dir.string());
    if (auto res = catalyst::install::action(install_args); !res) {
        return std::unexpected(std::format("Failed to stage artifacts for packing: {}", res.error()));
    }

    // 2. Extract configuration for CPack
    std::string pkg_name = config.getString("manifest.name").value_or("catalyst-pkg");
    std::string pkg_version = config.getString("manifest.version").value_or("1.0.0");
    std::string pkg_description = config.getString("manifest.description").value_or("A Catalyst packaged project");
    std::string pkg_contact = config.getString("manifest.maintainer").value_or(
        config.getString("manifest.author").value_or("Unknown <unknown@example.com>")
    );

    // 3. Generate CPackConfig.cmake
    fs::path cpack_config_path = build_dir / "CPackConfig.cmake";
    {
        std::ofstream cpack_out(cpack_config_path);
        if (!cpack_out) {
            return std::unexpected("Failed to open CPackConfig.cmake for writing.");
        }
        cpack_out << std::format("set(CPACK_PACKAGE_NAME \"{}\")\n", pkg_name);
        cpack_out << std::format("set(CPACK_PACKAGE_VERSION \"{}\")\n", pkg_version);
        cpack_out << std::format("set(CPACK_PACKAGE_FILE_NAME \"{}-{}\")\n", pkg_name, pkg_version);
        cpack_out << std::format("set(CPACK_PACKAGE_DESCRIPTION_SUMMARY \"{}\")\n", pkg_description);
        cpack_out << std::format("set(CPACK_PACKAGE_DESCRIPTION \"{}\")\n", pkg_description);
        cpack_out << std::format("set(CPACK_PACKAGE_CONTACT \"{}\")\n", pkg_contact);
        cpack_out << "set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)\n";
        cpack_out << std::format("set(CPACK_INSTALLED_DIRECTORIES \"{}\" \".\")\n", fs::absolute(staging_dir).string());
    }

    // 4. Run cpack
    std::vector<std::string> cpack_cmd{"cpack", "--config", cpack_config_path.string(), "-B", target_path.string()};
    if (!parse_args.generators.empty()) {
        std::string gens;
        auto gen_to_string = [](Generator g) -> std::string {
            switch (g) {
                case Generator::TGZ: return "TGZ";
                case Generator::ZIP: return "ZIP";
                case Generator::DEB: return "DEB";
                case Generator::RPM: return "RPM";
                default: return "TGZ";
            }
        };

        for (size_t i = 0; i < parse_args.generators.size(); ++i) {
            gens += gen_to_string(parse_args.generators[i]);
            if (i + 1 < parse_args.generators.size()) {
                gens += ";";
            }
        }
        cpack_cmd.emplace_back("-G");
        cpack_cmd.push_back(gens);
    }

    try {
        fs::create_directories(target_path);
    } catch (const std::exception &e) {
        return std::unexpected(std::format("Failed to create target directory: {}", e.what()));
    }

    catalyst::logger.info("Running cpack to assemble package in {}", target_path.string());
    if (auto res = catalyst::processExec(std::move(cpack_cmd)); !res) {
        return std::unexpected(std::format("Failed to execute cpack: {}", res.error()));
    } else {
        if (int code = res.value().get(); code != 0) {
            return std::unexpected(std::format("cpack failed with exit code {}", code));
        }
    }

    // 5. Cleanup
    catalyst::logger.debug("Cleaning up staging directory: {}", staging_dir.string());
    if (fs::exists(staging_dir)) {
        fs::remove_all(staging_dir);
    }
    if (fs::exists(cpack_config_path)) {
        fs::remove(cpack_config_path);
    }

    // 6. Normalize package permissions to 644
    for (const auto& entry : fs::directory_iterator(target_path)) {
        if (entry.is_regular_file()) {
            fs::permissions(entry.path(), 
                            fs::perms::owner_read | fs::perms::owner_write | 
                            fs::perms::group_read | fs::perms::others_read, 
                            fs::perm_options::replace);
        }
    }

    catalyst::logger.debug("Pack subcommand finished successfully.");
    return {};
}

} // namespace catalyst::pack
