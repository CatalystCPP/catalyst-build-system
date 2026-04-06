#include "catalyst/utils/toolchain.hpp"

#include <format>

#include <yaml-cpp/yaml.h>

namespace catalyst::toolchain {

std::string expand_template(std::string_view tmpl, const std::unordered_map<std::string_view, std::string> &vars) {
    std::string result;
    result.reserve(tmpl.size());
    size_t i = 0;
    while (i < tmpl.size()) {
        if (tmpl[i] == '{') {
            size_t end = tmpl.find('}', i);
            if (end != std::string_view::npos) {
                std::string_view key = tmpl.substr(i + 1, end - i - 1);
                if (auto it = vars.find(key); it != vars.end()) {
                    result.append(it->second);
                } else {
                    // Placeholder not found, keep it as is or expand to empty.
                    // Let's expand to empty, as missing flags should not leave {placeholder}
                }
                i = end + 1;
                continue;
            }
        }
        result.push_back(tmpl[i]);
        ++i;
    }
    return result;
}

std::expected<ToolchainDef, std::string> parse_toolchain(const std::filesystem::path &path) {
    ToolchainDef tc;
    try {
        YAML::Node root = YAML::LoadFile(path.string());
        if (!root["toolchain"]) {
            return std::unexpected(std::format("Toolchain file {} missing 'toolchain' root node.", path.string()));
        }
        YAML::Node node = root["toolchain"];

        if (node["name"])
            tc.name = node["name"].as<std::string>();

        if (YAML::Node ext = node["extensions"]) {
            if (ext["object"])
                tc.extensions.object = ext["object"].as<std::string>();
            if (ext["executable"])
                tc.extensions.executable = ext["executable"].as<std::string>();
            if (ext["static_lib"])
                tc.extensions.static_lib = ext["static_lib"].as<std::string>();
            if (ext["shared_lib"])
                tc.extensions.shared_lib = ext["shared_lib"].as<std::string>();
            if (ext["static_lib_prefix"])
                tc.extensions.static_lib_prefix = ext["static_lib_prefix"].as<std::string>();
            if (ext["shared_lib_prefix"])
                tc.extensions.shared_lib_prefix = ext["shared_lib_prefix"].as<std::string>();
        }

        if (YAML::Node flags = node["flags"]) {
            if (flags["include_dir"])
                tc.flags.include_dir = flags["include_dir"].as<std::string>();
            if (flags["lib_dir"])
                tc.flags.lib_dir = flags["lib_dir"].as<std::string>();
            if (flags["lib"])
                tc.flags.lib = flags["lib"].as<std::string>();
            if (flags["define"])
                tc.flags.define = flags["define"].as<std::string>();
            if (flags["define_empty"])
                tc.flags.define_empty = flags["define_empty"].as<std::string>();
        }

        if (YAML::Node comp = node["compiler"]) {
            if (YAML::Node c = comp["c"]) {
                if (c["executable"])
                    tc.compiler.c.executable = c["executable"].as<std::string>();
                if (c["command"])
                    tc.compiler.c.command = c["command"].as<std::string>();
            }
            if (YAML::Node cxx = comp["cxx"]) {
                if (cxx["executable"])
                    tc.compiler.cxx.executable = cxx["executable"].as<std::string>();
                if (cxx["command"])
                    tc.compiler.cxx.command = cxx["command"].as<std::string>();
            }
        }

        if (YAML::Node link = node["linker"]) {
            if (link["executable"])
                tc.linker.executable = link["executable"].as<std::string>();
            if (link["executable_command"])
                tc.linker.executable_command = link["executable_command"].as<std::string>();
            if (link["shared_lib_command"])
                tc.linker.shared_lib_command = link["shared_lib_command"].as<std::string>();
        }

        if (YAML::Node arch = node["archiver"]) {
            if (arch["executable"])
                tc.archiver.executable = arch["executable"].as<std::string>();
            if (arch["command"])
                tc.archiver.command = arch["command"].as<std::string>();
        }
    } catch (const YAML::Exception &e) {
        return std::unexpected(std::format("YAML parsing error in {}: {}", path.string(), e.what()));
    }
    return tc;
}

} // namespace catalyst::toolchain
