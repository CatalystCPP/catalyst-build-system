#include <expected>
#include <filesystem>
#include <format>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::generate {
namespace fs = std::filesystem;

std::expected<FindRes, std::string>
findConan(const std::string &build_dir, ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc) {
    namespace yaml = catalyst::utils::yaml;
    auto dep_name = yaml::asString(yaml::child(dep, "name")).value_or("<unnamed>");
    catalyst::logger.debug("Resolving Conan dependency: {}", dep_name);

    fs::path conan_dir = fs::path(build_dir) / "conan";
    if (!fs::exists(conan_dir) || !fs::is_directory(conan_dir)) {
        return std::unexpected(std::format("Conan build output directory '{}' not found. Did you run 'catalyst fetch'?",
                                           conan_dir.string()));
    }

    std::unordered_map<std::string, std::string> env;
    if (const char *path_env = std::getenv("PATH")) {
        env["PATH"] = path_env;
    }
    env["PKG_CONFIG_PATH"] = conan_dir.string();

    auto res_cflags = processExecStdout({"pkg-config", "--cflags", dep_name}, std::nullopt, env);
    auto res_L = processExecStdout({"pkg-config", "--libs-only-L", dep_name}, std::nullopt, env);
    auto res_l = processExecStdout({"pkg-config", "--libs-only-l", "--libs-only-other", dep_name}, std::nullopt, env);

    if (!(res_cflags && res_L && res_l)) {
        return std::unexpected(std::format("pkg-config failed to resolve Conan package '{}'.", dep_name));
    }

    FindRes result{.lib_path = "", .inc_path = "", .libs = "", .lib_dirs = {}};
    auto append_tokens = [&](const std::string &value, auto &&handler) {
        std::stringstream stream(value);
        std::string token;
        while (stream >> token) {
            handler(token);
        }
    };

    append_tokens(*res_cflags, [&](const std::string &token) {
        if (token.rfind("-I", 0) == 0) {
            result.inc_path +=
                " " + catalyst::toolchain::expand_template(tc.flags.include_dir, {{"path", token.substr(2)}});
        } else {
            result.inc_path += " " + token;
        }
    });
    append_tokens(*res_L, [&](const std::string &token) {
        if (token.rfind("-L", 0) == 0) {
            std::string path = token.substr(2);
            result.lib_path += " " + catalyst::toolchain::expand_template(tc.flags.lib_dir, {{"path", path}});
            result.lib_dirs.push_back(path);
        } else {
            result.lib_path += " " + token;
        }
    });
    append_tokens(*res_l, [&](const std::string &token) {
        if (token.rfind("-l", 0) == 0) {
            result.libs += " " + catalyst::toolchain::expand_template(tc.flags.lib, {{"name", token.substr(2)}});
        } else {
            result.libs += " " + token;
        }
    });

    catalyst::logger.debug("Resolved Conan package '{}' via pkg-config: cflags='{}' L='{}' l='{}'",
                           dep_name,
                           result.inc_path,
                           result.lib_path,
                           result.libs);
    return result;
}
} // namespace catalyst::generate
