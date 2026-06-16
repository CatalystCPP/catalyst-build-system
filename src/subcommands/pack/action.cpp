#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include <catalyst/hooks.hpp>

#include "catalyst/dir_guard.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/install.hpp"
#include "catalyst/subcommands/pack.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::pack {
namespace fs = std::filesystem;

namespace {
std::string escape_cmake(const std::string &str) {
    std::string res;
    for (char c : str) {
        if (c == '\\' || c == '"' || c == ';') {
            res += '\\';
            res += c;
        } else if (c == '\n') {
            res += "\\n";
        } else {
            res += c;
        }
    }
    return res;
}

struct CleanupGuard {
    std::vector<fs::path> paths;
    ~CleanupGuard() {
        for (const auto &p : paths) {
            std::error_code ec;
            if (fs::exists(p, ec)) {
                fs::remove_all(p, ec);
            }
        }
    }
};
} // namespace

Result<void> action(const Parse &parse_args) {
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

    catalyst::logger.debug("Running pre-pack hooks.");
    if (auto res = hooks::prePack(config); !res) {
        return res;
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
    std::error_code ec;
    if (fs::exists(staging_dir, ec)) {
        fs::remove_all(staging_dir, ec);
    }

    CleanupGuard cleanup_guard;
    cleanup_guard.paths.push_back(staging_dir);

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
    std::string pkg_contact =
        config.getString("manifest.maintainer")
            .value_or(config.getString("manifest.author").value_or("Unknown <unknown@example.com>"));
    std::string pkg_vendor = config.getString("manifest.vendor").value_or("Catalyst");

    // 3. Generate CPackConfig.cmake
    fs::path cpack_config_path = build_dir / "CPackConfig.cmake";
    cleanup_guard.paths.push_back(cpack_config_path);

    {
        std::ofstream cpack_out(cpack_config_path);
        if (!cpack_out) {
            return std::unexpected("Failed to open CPackConfig.cmake for writing.");
        }
        cpack_out << std::format("set(CPACK_PACKAGE_NAME \"{}\")\n", escape_cmake(pkg_name));
        cpack_out << std::format("set(CPACK_PACKAGE_VERSION \"{}\")\n", escape_cmake(pkg_version));
        cpack_out << std::format(
            "set(CPACK_PACKAGE_FILE_NAME \"{}-{}\")\n", escape_cmake(pkg_name), escape_cmake(pkg_version));
        cpack_out << std::format("set(CPACK_PACKAGE_DESCRIPTION_SUMMARY \"{}\")\n", escape_cmake(pkg_description));
        cpack_out << std::format("set(CPACK_PACKAGE_DESCRIPTION \"{}\")\n", escape_cmake(pkg_description));
        cpack_out << std::format("set(CPACK_PACKAGE_CONTACT \"{}\")\n", escape_cmake(pkg_contact));
        cpack_out << std::format("set(CPACK_PACKAGE_VENDOR \"{}\")\n", escape_cmake(pkg_vendor));

        if (auto license = config.getString("manifest.license_file"); license) {
            cpack_out << std::format("set(CPACK_RESOURCE_FILE_LICENSE \"{}\")\n",
                                     escape_cmake(fs::absolute(*license).generic_string()));
        }

        if (auto readme = config.getString("manifest.readme_file"); readme) {
            cpack_out << std::format("set(CPACK_RESOURCE_FILE_README \"{}\")\n",
                                     escape_cmake(fs::absolute(*readme).generic_string()));
        }

        cpack_out << "set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)\n";
        cpack_out << std::format("set(CPACK_INSTALLED_DIRECTORIES \"{}\" \".\")\n",
                                 escape_cmake(fs::absolute(staging_dir).generic_string()));
        cpack_out << "set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION \"/bin\" \"/usr\" \"/usr/bin\" \"/usr/lib\" "
                     "\"/usr/lib64\" \"/etc\")\n";
    }

    // 4. Run cpack
    auto gen_to_string = [](Generator g) -> std::string {
        switch (g) {
            case Generator::TGZ:
                return "TGZ";
            case Generator::ZIP:
                return "ZIP";
            case Generator::DEB:
                return "DEB";
            case Generator::RPM:
                return "RPM";
            case Generator::NSIS:
                return "NSIS";
            case Generator::WIX:
                return "WIX";
            case Generator::DMG:
                return "DragNDrop";
            case Generator::STGZ:
                return "STGZ";
            case Generator::FREEBSD:
                return "FREEBSD";
            case Generator::APK:
                return "APK";
            case Generator::SEVEN_ZIP:
                return "7Z";
            case Generator::TXZ:
                return "TXZ";
            case Generator::EXTERNAL:
                return "External";
            default:
                return "TGZ";
        }
    };

    std::vector<Generator> active_generators = parse_args.generators;
    if (parse_args.all_generators) {
        active_generators = {Generator::TGZ,
                             Generator::ZIP,
                             Generator::DEB,
                             Generator::RPM,
                             Generator::NSIS,
                             Generator::WIX,
                             Generator::DMG,
                             Generator::STGZ,
                             Generator::FREEBSD,
                             Generator::APK,
                             Generator::SEVEN_ZIP,
                             Generator::TXZ};
    }

    try {
        fs::create_directories(target_path);
    } catch (const std::exception &e) {
        return std::unexpected(std::format("Failed to create target directory: {}", e.what()));
    }

    std::vector<std::string> base_cpack_cmd{
        "cpack", "--config", cpack_config_path.string(), "-B", target_path.string()};

    if (active_generators.empty()) {
        catalyst::logger.info("Running cpack to assemble package in {}", target_path.string());
        if (auto res = catalyst::processExec(
                std::vector<std::string>(base_cpack_cmd), std::nullopt, std::nullopt, parse_args.silent);
            !res) {
            return std::unexpected(std::format("Failed to execute cpack: {}", res.error()));
        } else {
            if (int code = res.value().get(); code != 0) {
                return std::unexpected(std::format("cpack failed with exit code {}", code));
            }
        }
    } else {
        catalyst::logger.info("Running cpack generators in parallel to assemble packages in {}", target_path.string());
        std::vector<std::future<int>> futures;
        std::vector<std::string> gen_names;

        for (auto g : active_generators) {
            std::string gen_str = gen_to_string(g);
            gen_names.push_back(gen_str);
            std::vector<std::string> cmd = base_cpack_cmd;
            cmd.push_back("-G");
            cmd.push_back(gen_str);

            auto res = catalyst::processExec(std::move(cmd), std::nullopt, std::nullopt, parse_args.silent);
            if (!res) {
                return std::unexpected(std::format("Failed to spawn cpack for generator {}: {}", gen_str, res.error()));
            }
            futures.push_back(std::move(res.value()));
        }

        bool has_error = false;
        int success_count = 0;
        for (size_t i = 0; i < futures.size(); ++i) {
            int code = futures[i].get();
            if (code != 0) {
                if (parse_args.all_generators) {
                    catalyst::logger.warn(
                        "cpack generator {} failed with exit code {} (ignored due to --all)", gen_names[i], code);
                } else {
                    catalyst::logger.error("cpack generator {} failed with exit code {}", gen_names[i], code);
                    has_error = true;
                }
            } else {
                success_count++;
            }
        }

        if (has_error) {
            return std::unexpected("One or more cpack generators failed.");
        }

        if (parse_args.all_generators && success_count == 0) {
            return std::unexpected("All cpack generators failed.");
        }
    }

    catalyst::logger.debug("Running post-pack hooks.");
    if (auto res = hooks::postPack(config); !res) {
        return res;
    }

    catalyst::logger.debug("Pack subcommand finished successfully.");
    return {};
}

} // namespace catalyst::pack
