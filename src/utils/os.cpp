#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <tuple>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/os/os_defs.hpp"

namespace catalyst::utils::os {

bool isCommandInstalled(const std::string &command) {
    if (command == "cob") {
        return true;
    }
    // 1. Try checking PATH
    const char *path_env = std::getenv("PATH");
    if (path_env) {
        std::string path_str = path_env;
        char delimiter = ':';
#ifdef _WIN32
        delimiter = ';';
#endif
        std::string token;
        std::istringstream token_stream(path_str);
        while (std::getline(token_stream, token, delimiter)) {
            if (token.empty())
                continue;
            try {
                std::filesystem::path dir(token);
                std::filesystem::path file_path = dir / command;
#ifdef _WIN32
                if (std::filesystem::exists(file_path) || std::filesystem::exists(file_path.string() + ".exe")
                    || std::filesystem::exists(file_path.string() + ".bat")
                    || std::filesystem::exists(file_path.string() + ".cmd")) {
                    return true;
                }
#else
                if (std::filesystem::exists(file_path) && !std::filesystem::is_directory(file_path)) {
                    return true;
                }
#endif
            } catch (std::exception &e) {
                catalyst::logger.warn("Exception occured trying to find command: {}, {}", command, e.what());
            } catch (...) {
                catalyst::logger.warn("Unknown exception occured trying to find command: {}", command);
            }
        }
    }

    // 2. Special case for vcpkg: check VCPKG_ROOT
    if (command == "vcpkg") {
        const char *vcpkg_root = std::getenv("VCPKG_ROOT");
        if (vcpkg_root) {
            try {
                std::filesystem::path root_path(vcpkg_root);
                std::filesystem::path vcpkg_bin = root_path / "vcpkg";
#ifdef _WIN32
                if (std::filesystem::exists(vcpkg_bin) || std::filesystem::exists(vcpkg_bin.string() + ".exe")) {
                    return true;
                }
#else
                if (std::filesystem::exists(vcpkg_bin) && !std::filesystem::is_directory(vcpkg_bin)) {
                    return true;
                }
#endif
            } catch (std::exception &e) {
                catalyst::logger.warn("Exception occured trying to find vcpkg command: {}", e.what());
            } catch (...) {
                catalyst::logger.warn("Unknown exception occured trying to find vcpkg command");
            }
        }
    }

    return false;
}

} // namespace catalyst::utils::os
