#include <expected>
#include <format>
#include <string>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

using std::string;

auto catalyst::generate::findDep(const std::string &build_dir,
                                 ryml::ConstNodeRef dep,
                                 const catalyst::toolchain::ToolchainDef &tc) -> Result<FindRes> {
    namespace yaml = catalyst::utils::yaml;
    auto source_type = yaml::asString(yaml::child(dep, "source"));
    if (!source_type)
        return std::unexpected(std::format("dependency: {} does not define field src",
                                           yaml::asString(yaml::child(dep, "name")).value_or("<unnamed>")));
    if (*source_type == "local") {
        return findLocal(dep, tc);
    }
    if (*source_type == "system") {
        return findSystem(dep, tc);
    }
    if (*source_type == "vcpkg") {
        return findVcpkg(dep, tc);
    }
    if (*source_type == "git") {
        return findGit(build_dir, dep, tc);
    }
    if (*source_type == "conan") {
        return findConan(build_dir, dep, tc);
    }
    if (*source_type == "custom") {
        return findCustom(dep, tc);
    }
    return std::unexpected(std::format("Unkown source_type: {}", *source_type));
}
