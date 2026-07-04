#include <algorithm>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace fs = std::filesystem;

namespace {

bool isCxxSource(const fs::path &src) {
    const std::string ext = src.extension().string();
    return ext == ".cpp" || ext == ".cxx" || ext == ".cc" || catalyst::generate::modules::isInterfaceExtension(src);
}

// Whitespace-split a flags string into argv elements for the scanner
// invocation. Quoting inside flags is not interpreted; the scan only needs
// include dirs / -std / defines to resolve imports.
std::vector<std::string> splitFlags(std::string_view flags) {
    std::vector<std::string> args;
    size_t pos = 0;
    while (pos < flags.size()) {
        size_t start = flags.find_first_not_of(" \t", pos);
        if (start == std::string_view::npos)
            break;
        size_t end = flags.find_first_of(" \t", start);
        if (end == std::string_view::npos)
            end = flags.size();
        args.emplace_back(flags.substr(start, end - start));
        pos = end;
    }
    return args;
}

} // namespace

namespace catalyst::generate::modules {

bool isInterfaceExtension(const fs::path &src) {
    const std::string ext = src.extension().string();
    return ext == ".cppm" || ext == ".ixx" || ext == ".mpp" || ext == ".cxxm";
}

bool needsExplicitModuleType(const fs::path &src) {
    const std::string ext = src.extension().string();
    return ext != ".cppm" && ext != ".cxxm";
}

std::string bmiPath(std::string_view module_name) {
    // Partition names contain ':', which is unfriendly to build files; map it
    // to '-' (module names themselves cannot contain '-').
    std::string name{module_name};
    std::ranges::replace(name, ':', '-');
    return (fs::path{"obj"} / (name + ".pcm")).string();
}

Result<ModuleInfo> parseP1689(std::string_view json) {
    namespace yaml = catalyst::utils::yaml;
    auto tree = yaml::parseYaml(json, "<clang-scan-deps p1689>"); // rapidyaml parses JSON
    if (!tree) {
        return std::unexpected(std::format("Failed to parse P1689 output: {}", tree.error()));
    }

    ModuleInfo info;
    ryml::ConstNodeRef rules = yaml::child(tree->crootref(), "rules");
    if (!rules.readable() || !rules.is_seq()) {
        return info; // no rules -> TU touches no modules
    }
    for (ryml::ConstNodeRef rule : rules.children()) {
        if (ryml::ConstNodeRef provides = yaml::child(rule, "provides"); provides.readable() && provides.is_seq()) {
            for (ryml::ConstNodeRef p : provides.children()) {
                if (auto name = yaml::asString(yaml::child(p, "logical-name"))) {
                    if (!info.provides.empty() && info.provides != *name) {
                        return std::unexpected(
                            std::format("TU provides multiple modules ('{}' and '{}')", info.provides, *name));
                    }
                    info.provides = *name;
                }
            }
        }
        if (ryml::ConstNodeRef requires_node = yaml::child(rule, "requires");
            requires_node.readable() && requires_node.is_seq()) {
            for (ryml::ConstNodeRef r : requires_node.children()) {
                if (auto name = yaml::asString(yaml::child(r, "logical-name"))) {
                    if (std::ranges::find(info.required, *name) == info.required.end()) {
                        info.required.push_back(*name);
                    }
                }
            }
        }
    }
    return info;
}

Result<ScanResult> scanModules(const std::unordered_set<fs::path> &source_set,
                               const catalyst::toolchain::ToolchainDef &tc,
                               std::string_view cxxflags) {
    ScanResult scan;

    // Modules only enter a project through an interface unit; without one
    // there is nothing to resolve, and we avoid requiring clang-scan-deps.
    if (std::ranges::none_of(source_set, [](const fs::path &src) { return isInterfaceExtension(src); })) {
        return scan;
    }

    catalyst::logger.debug("Module interface unit(s) found; running P1689 dependency scan.");
    const std::vector<std::string> flag_args = splitFlags(cxxflags);
    const fs::path current_dir = fs::current_path();

    for (const auto &src : source_set) {
        if (!isCxxSource(src))
            continue;

        std::vector<std::string> cmd = {"clang-scan-deps", "-format=p1689", "--", tc.compiler.cxx.executable};
        cmd.insert(cmd.end(), flag_args.begin(), flag_args.end());
        if (isInterfaceExtension(src) && needsExplicitModuleType(src)) {
            cmd.emplace_back("-x");
            cmd.emplace_back("c++-module");
        }
        cmd.emplace_back("-c");
        cmd.emplace_back(src.string());
        cmd.emplace_back("-o");
        cmd.emplace_back((fs::path{"obj"} / src.filename().replace_extension(tc.extensions.object)).string());

        auto output = catalyst::processExecStdout(cmd);
        if (!output) {
            return std::unexpected(
                std::format("Module dependency scan failed for {} (is clang-scan-deps installed?): {}",
                            src.string(),
                            output.error()));
        }
        auto info = parseP1689(*output);
        if (!info) {
            return std::unexpected(std::format("{} while scanning {}", info.error(), src.string()));
        }
        if (info->provides.empty() && info->required.empty())
            continue;

        if (!info->provides.empty()) {
            auto [it, inserted] = scan.providers.emplace(info->provides, src);
            if (!inserted) {
                return std::unexpected(std::format(
                    "Module '{}' is provided by both {} and {}", info->provides, it->second.string(), src.string()));
            }
            catalyst::logger.debug("{} provides module '{}'", src.string(), info->provides);
        }
        scan.tu_modules.emplace(src, std::move(*info));
    }

    // Every required module must have a provider in the set. External or
    // prebuilt modules (import std;, header units) are out of scope for now.
    for (const auto &[src, info] : scan.tu_modules) {
        for (const auto &req : info.required) {
            if (!scan.providers.contains(req)) {
                return std::unexpected(std::format("{} imports module '{}', but no source in the set provides it "
                                                   "(external/prebuilt modules are not supported yet)",
                                                   fs::relative(src, current_dir).string(),
                                                   req));
            }
        }
    }

    return scan;
}

} // namespace catalyst::generate::modules
