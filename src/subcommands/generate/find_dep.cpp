#include <expected>
#include <format>
#include <string>

#include "catalyst/subcommands/generate.hpp"

#include "yaml-cpp/node/node.h"

using std::string;

namespace catalyst::generate {
std::expected<FindRes, std::string>
findDep(const std::string &build_dir, const YAML::Node &dep, const catalyst::toolchain::ToolchainDef &tc) {
    if (!dep["source"] || !dep["source"].IsScalar())
        return std::unexpected(std::format("dependency: {} does not define field src", dep["name"].as<std::string>()));
    auto source_type = dep["source"].as<std::string>();
    if (source_type == "local") {
        return findLocal(dep, tc);
    }
    if (source_type == "system") {
        return findSystem(dep, tc);
    }
    if (source_type == "vcpkg") {
        return findVcpkg(dep, tc);
    }
    if (source_type == "git") {
        return findGit(build_dir, dep, tc);
    }
    return std::unexpected(std::format("Unkown source_type: {}", source_type));
}
} // namespace catalyst::generate
