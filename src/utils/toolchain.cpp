#include "catalyst/utils/toolchain.hpp"

#include <algorithm>
#include <format>
#include <vector>

#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

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

Result<void>
parse_toolchain_impl(const std::filesystem::path &path, ToolchainDef &tc, std::vector<std::filesystem::path> &visited) {
    namespace yaml = catalyst::utils::yaml;

    std::filesystem::path canonical_path;
    try {
        canonical_path = std::filesystem::canonical(path);
    } catch (...) {
        canonical_path = std::filesystem::absolute(path);
    }

    // Cycle detection
    auto it = std::find(visited.begin(), visited.end(), canonical_path);
    if (it != visited.end()) {
        std::string cycle_str;
        for (const auto &p : visited) {
            cycle_str += p.string() + " -> ";
        }
        cycle_str += canonical_path.string();
        return std::unexpected(std::format("Toolchain inheritance cycle detected: {}", cycle_str));
    }

    if (visited.size() >= 32) {
        return std::unexpected("Toolchain inheritance depth limit exceeded");
    }

    auto tree = yaml::loadFile(canonical_path);
    if (!tree) {
        if (!visited.empty()) {
            return std::unexpected(std::format("Failed to open base toolchain '{}' referenced by '{}': {}",
                                               canonical_path.string(),
                                               visited.back().string(),
                                               tree.error()));
        }
        return std::unexpected(std::format("YAML parsing error in {}: {}", canonical_path.string(), tree.error()));
    }

    ryml::ConstNodeRef node = yaml::child(tree->crootref(), "toolchain");
    if (!node.readable()) {
        return std::unexpected(
            std::format("Toolchain file {} missing 'toolchain' root node.", canonical_path.string()));
    }

    visited.push_back(canonical_path);

    ryml::ConstNodeRef extends_node = yaml::child(node, "extends");
    if (extends_node.readable()) {
        auto val = yaml::asString(extends_node);
        if (!val) {
            visited.pop_back();
            return std::unexpected(
                std::format("toolchain.extends must be a string path in '{}'", canonical_path.string()));
        }
        std::filesystem::path extends_path = *val;
        std::filesystem::path base_path =
            extends_path.is_absolute() ? extends_path : canonical_path.parent_path() / extends_path;

        auto res = parse_toolchain_impl(base_path, tc, visited);
        if (!res) {
            visited.pop_back();
            return std::unexpected(res.error());
        }
    }

    // Assigns the scalar child `key` of `parent` to `target`, if present.
    auto set = [](ryml::ConstNodeRef parent, std::string_view key, std::string &target) {
        if (auto val = yaml::asString(yaml::child(parent, key)))
            target = std::move(*val);
    };

    set(node, "name", tc.name);

    ryml::ConstNodeRef ext = yaml::child(node, "extensions");
    set(ext, "object", tc.extensions.object);
    set(ext, "executable", tc.extensions.executable);
    set(ext, "static_lib", tc.extensions.static_lib);
    set(ext, "shared_lib", tc.extensions.shared_lib);
    set(ext, "static_lib_prefix", tc.extensions.static_lib_prefix);
    set(ext, "shared_lib_prefix", tc.extensions.shared_lib_prefix);

    ryml::ConstNodeRef flags = yaml::child(node, "flags");
    set(flags, "include_dir", tc.flags.include_dir);
    set(flags, "lib_dir", tc.flags.lib_dir);
    set(flags, "lib", tc.flags.lib);
    set(flags, "define", tc.flags.define);
    set(flags, "define_empty", tc.flags.define_empty);

    ryml::ConstNodeRef comp = yaml::child(node, "compiler");
    ryml::ConstNodeRef comp_c = yaml::child(comp, "c");
    set(comp_c, "executable", tc.compiler.c.executable);
    set(comp_c, "flags", tc.compiler.c.flags);
    set(comp_c, "command", tc.compiler.c.command);
    ryml::ConstNodeRef comp_cxx = yaml::child(comp, "cxx");
    set(comp_cxx, "executable", tc.compiler.cxx.executable);
    set(comp_cxx, "flags", tc.compiler.cxx.flags);
    set(comp_cxx, "command", tc.compiler.cxx.command);

    ryml::ConstNodeRef link = yaml::child(node, "linker");
    set(link, "executable", tc.linker.executable);
    set(link, "flags", tc.linker.flags);
    set(link, "executable_command", tc.linker.executable_command);
    set(link, "shared_lib_command", tc.linker.shared_lib_command);

    ryml::ConstNodeRef arch = yaml::child(node, "archiver");
    set(arch, "executable", tc.archiver.executable);
    set(arch, "command", tc.archiver.command);

    visited.pop_back();
    return {};
}

Result<ToolchainDef> parse_toolchain(const std::filesystem::path &path) {
    ToolchainDef tc;
    std::vector<std::filesystem::path> visited;
    auto res = parse_toolchain_impl(path, tc, visited);
    if (!res) {
        return std::unexpected(res.error());
    }
    return tc;
}

} // namespace catalyst::toolchain
