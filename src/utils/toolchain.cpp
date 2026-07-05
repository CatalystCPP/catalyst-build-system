#include "catalyst/utils/toolchain.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <vector>

#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

static constexpr size_t TUNABLE_MAX_INHERITANCE_DEPTH = 32;

namespace catalyst::toolchain {

namespace {

std::vector<std::string> splitTokens(std::string_view str) {
    auto is_space = [](char c) constexpr -> bool { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    return str | std::views::chunk_by([&is_space](char a, char b) { return is_space(a) == is_space(b); })
           | std::views::filter([&is_space](auto chunk) { return !is_space(chunk.front()); })
           | std::views::transform([](auto chunk) { return std::string(std::string_view(chunk)); })
           | std::ranges::to<std::vector<std::string>>();
}

void modifyFlags(ryml::ConstNodeRef parent, std::string &flags) {
    namespace yaml = catalyst::utils::yaml;

    // because flags can be specified as either a string or a sequence of strings, we need to handle both cases and
    // return a joined string of flags. If the node is not readable, we return std::nullopt.
    auto get_flags_string = [](ryml::ConstNodeRef parent_node,
                               std::string_view key) constexpr -> std::optional<std::string> {
        auto node = yaml::child(parent_node, key);
        if (!node.readable()) {
            return std::nullopt;
        }
        if (auto vec = yaml::asStringVector(node)) {
            using namespace std::string_view_literals;
            return *vec | std::views::join_with(" "sv) | std::ranges::to<std::string>();
        }
        return yaml::asString(node);
    };

    // 1. Wholesale overwrite (if 'flags' is specified)
    if (auto val = get_flags_string(parent, "flags")) {
        flags = std::move(*val);
    }

    // 2. Token-based removal (if 'flags_remove' is specified)
    if (auto val = get_flags_string(parent, "flags_remove")) {
        using namespace std::string_view_literals;
        auto to_remove = splitTokens(*val);
        flags =
            splitTokens(flags)
            | std::views::filter([&to_remove](const std::string &t) { return !std::ranges::contains(to_remove, t); })
            | std::views::join_with(" "sv) | std::ranges::to<std::string>();
    }

    // 3. Append (if 'flags_append' is specified)
    if (auto val = get_flags_string(parent, "flags_append")) {
        if (!flags.empty() && !val->empty()) {
            flags += " ";
        }
        flags += *val;
    }
}

Result<void>
parseToolchainImpl(const std::filesystem::path &path, ToolchainDef &tc, std::vector<std::filesystem::path> &visited) {
    namespace yaml = catalyst::utils::yaml;

    std::error_code ec;
    std::filesystem::path canonical_path;
    canonical_path = std::filesystem::canonical(path, ec);
    if (ec) {
        canonical_path = std::filesystem::absolute(path);
    }

    // Cycle detection
    if (std::ranges::contains(visited, canonical_path)) {
        std::string cycle_str;
        for (const auto &p : visited) {
            cycle_str += p.string() + " -> ";
        }
        cycle_str += canonical_path.string();
        return std::unexpected(std::format("Toolchain inheritance cycle detected: {}", cycle_str));
    }

    if (visited.size() >= TUNABLE_MAX_INHERITANCE_DEPTH) {
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

        auto res = parseToolchainImpl(base_path, tc, visited);
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

    // Assigns the sequence child `key` of `parent` to `target`, if present.
    auto set_vector = [](ryml::ConstNodeRef parent, std::string_view key, std::vector<std::string> &target) {
        if (auto val = yaml::asStringVector(yaml::child(parent, key)))
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

    set_vector(ext, "cpp_sources", tc.extensions.cpp_sources);
    set_vector(ext, "headers", tc.extensions.headers);
    set_vector(ext, "c_sources", tc.extensions.c_sources);
    set_vector(ext, "module_interfaces", tc.extensions.module_interfaces);
    set_vector(ext, "clang_modules", tc.extensions.clang_modules);
    set(ext, "bmi", tc.extensions.bmi);
    set_vector(ext, "library_scan", tc.extensions.library_scan);
    set_vector(ext, "shell_scripts", tc.extensions.shell_scripts);

    ryml::ConstNodeRef flags = yaml::child(node, "flags");
    set(flags, "include_dir", tc.flags.include_dir);
    set(flags, "lib_dir", tc.flags.lib_dir);
    set(flags, "lib", tc.flags.lib);
    set(flags, "define", tc.flags.define);
    set(flags, "define_empty", tc.flags.define_empty);

    ryml::ConstNodeRef comp = yaml::child(node, "compiler");
    ryml::ConstNodeRef comp_c = yaml::child(comp, "c");
    set(comp_c, "executable", tc.compiler.c.executable);
    modifyFlags(comp_c, tc.compiler.c.flags);
    set(comp_c, "command", tc.compiler.c.command);
    ryml::ConstNodeRef comp_cxx = yaml::child(comp, "cxx");
    set(comp_cxx, "executable", tc.compiler.cxx.executable);
    modifyFlags(comp_cxx, tc.compiler.cxx.flags);
    set(comp_cxx, "command", tc.compiler.cxx.command);

    ryml::ConstNodeRef link = yaml::child(node, "linker");
    set(link, "executable", tc.linker.executable);
    modifyFlags(link, tc.linker.flags);
    set(link, "executable_command", tc.linker.executable_command);
    set(link, "shared_lib_command", tc.linker.shared_lib_command);

    ryml::ConstNodeRef arch = yaml::child(node, "archiver");
    set(arch, "executable", tc.archiver.executable);
    set(arch, "command", tc.archiver.command);

    visited.pop_back();
    return {};
}

} // namespace

std::string expandTemplate(std::string_view tmpl, const std::unordered_map<std::string_view, std::string> &vars) {
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

Result<ToolchainDef> parseToolchain(const std::filesystem::path &path) {
    ToolchainDef tc;
    std::vector<std::filesystem::path> visited;
    auto res = parseToolchainImpl(path, tc, visited);
    if (!res) {
        return std::unexpected(res.error());
    }
    if (tc.extensions.library_scan.empty()) {
#if defined(_WIN32)
        tc.extensions.library_scan = {".lib"};
#elif defined(__APPLE__)
        tc.extensions.library_scan = {".a", ".dylib"};
#else
        tc.extensions.library_scan = {".a", ".so"};
#endif
    }
    return tc;
}

} // namespace catalyst::toolchain
