#include <charconv>
#include <cstdlib>
#include <expected>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ryml/ryml.hpp>
#include <ryml/ryml_std.hpp>

#include "catalyst/subcommands/introspect.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::introspect {
namespace {

struct PathToken {
    std::string text;
    bool bracket_index{false};
};

constexpr std::size_t JSON_INDENTATION = 2;

Result<std::vector<PathToken>> tokenizePath(std::string_view path) {
    std::vector<PathToken> tokens;
    std::size_t position = 0;
    while (position < path.size()) {
        if (path[position] == '.')
            return std::unexpected("empty path component");

        if (path[position] != '[') {
            const std::size_t begin = position;
            while (position < path.size() && path[position] != '.' && path[position] != '[')
                ++position;
            tokens.push_back({.text = std::string{path.substr(begin, position - begin)}});
        }

        while (position < path.size() && path[position] == '[') {
            const std::size_t begin = ++position;
            while (position < path.size() && path[position] >= '0' && path[position] <= '9')
                ++position;
            if (begin == position || position == path.size() || path[position] != ']')
                return std::unexpected("invalid bracket index");
            tokens.push_back({.text = std::string{path.substr(begin, position - begin)}, .bracket_index = true});
            ++position;
        }

        if (position == path.size())
            break;
        if (path[position] != '.')
            return std::unexpected("invalid path separator");
        ++position;
        if (position == path.size())
            return std::unexpected("empty path component");
    }
    return tokens;
}

std::optional<std::size_t> parseIndex(std::string_view text) {
    std::size_t index = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), index);
    if (error != std::errc{} || end != text.data() + text.size())
        return std::nullopt;
    return index;
}

Result<ryml::ConstNodeRef> queryPath(ryml::ConstNodeRef root, std::string_view path) {
    auto tokens = tokenizePath(path);
    if (!tokens)
        return std::unexpected(std::format("key '{}' not found", path));

    ryml::ConstNodeRef current = root;
    for (const PathToken &token : *tokens) {
        if (current.is_seq()) {
            auto index = parseIndex(token.text);
            if (!index)
                return std::unexpected(std::format("key '{}' not found", path));
            if (*index >= current.num_children())
                return std::unexpected(std::format("index {} out of bounds", *index));
            current = current[*index];
        } else if (current.is_map() && !token.bracket_index) {
            current = catalyst::utils::yaml::child(current, token.text);
            if (!current.readable())
                return std::unexpected(std::format("key '{}' not found", path));
        } else {
            return std::unexpected(std::format("key '{}' not found", path));
        }
    }
    return current;
}

std::string compactJson(std::string_view json) {
    std::string compact;
    compact.reserve(json.size());
    bool in_string = false;
    bool escaped = false;
    for (const char character : json) {
        if (in_string) {
            compact.push_back(character);
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                in_string = false;
            }
        } else if (character == '"') {
            in_string = true;
            compact.push_back(character);
        } else if (character != ' ' && character != '\t' && character != '\r' && character != '\n') {
            compact.push_back(character);
        }
    }
    return compact;
}

std::string prettyJson(std::string_view compact_json) {
    std::string output;
    output.reserve(compact_json.size() + (compact_json.size() / 4));
    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;

    auto indent = [&output, &depth]() { output.append(depth * JSON_INDENTATION, ' '); };
    for (std::size_t index = 0; index < compact_json.size(); ++index) {
        const char character = compact_json[index];
        if (in_string) {
            output.push_back(character);
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                in_string = false;
            }
            continue;
        }
        if (character == '"') {
            in_string = true;
            output.push_back(character);
        } else if (character == '{' || character == '[') {
            output.push_back(character);
            const bool has_next_character = index + 1 < compact_json.size();
            const bool is_empty_object = has_next_character && character == '{' && compact_json[index + 1] == '}';
            const bool is_empty_array = has_next_character && character == '[' && compact_json[index + 1] == ']';
            if (has_next_character && !is_empty_object && !is_empty_array) {
                output.push_back('\n');
                ++depth;
                indent();
            }
        } else if (character == '}' || character == ']') {
            const char opening = character == '}' ? '{' : '[';
            if (index > 0 && compact_json[index - 1] != opening) {
                output.push_back('\n');
                --depth;
                indent();
            }
            output.push_back(character);
        } else if (character == ',') {
            output += ",\n";
            indent();
        } else if (character == ':') {
            output += ": ";
        } else {
            output.push_back(character);
        }
    }
    return output;
}

std::string emitContainer(ryml::ConstNodeRef node, bool pretty) {
    ryml::Tree value;
    value.rootref() |= node.is_seq() ? ryml::SEQ : ryml::MAP;
    for (ryml::ConstNodeRef child : node.children())
        catalyst::utils::yaml::appendCopy(value.rootref(), child);
    std::string json = compactJson(ryml::emitrs_json<std::string>(value));
    return pretty ? prettyJson(json) : json;
}

Result<ryml::Tree> loadSnapshot(std::string_view path) {
    std::ifstream input{std::string{path}, std::ios::binary};
    if (!input)
        return std::unexpected(std::format("failed to open state file at '{}'", path));
    std::ostringstream contents;
    contents << input.rdbuf();
    if (input.bad())
        return std::unexpected(std::format("failed to open state file at '{}'", path));
    auto tree = catalyst::utils::yaml::parseYaml(contents.view(), path);
    if (!tree)
        return std::unexpected(std::format("failed to parse state file: {}", tree.error()));
    return tree;
}

} // namespace

Result<void> action(const Parse &parse_res) {
    const char *hook_guard = std::getenv("CATALYST_HOOK");
    if (hook_guard == nullptr || std::string_view{hook_guard} != "1") {
        return std::unexpected("catalyst introspect: error: can only be executed within a Catalyst lifecycle hook "
                               "(CATALYST_HOOK=1 is not set)");
    }

    const char *snapshot_path_environment = std::getenv("CATALYST_INTROSPECT_FILE");
    const std::string_view snapshot_path = snapshot_path_environment == nullptr ? "" : snapshot_path_environment;
    auto snapshot = loadSnapshot(snapshot_path);
    if (!snapshot)
        return std::unexpected("catalyst introspect: error: " + snapshot.error());

    ryml::ConstNodeRef value = snapshot->crootref();
    if (!parse_res.path.empty()) {
        auto queried_value = queryPath(value, parse_res.path);
        if (!queried_value)
            return std::unexpected("catalyst introspect: " + queried_value.error());
        value = *queried_value;
    }

    if (value.is_map() || value.is_seq()) {
        std::println("{}", emitContainer(value, parse_res.pretty));
    } else if (value.has_val() && !value.val_is_null()) {
        if (!value.is_val_quoted()) {
            if (auto boolean = catalyst::utils::yaml::asBool(value)) {
                std::println("{}", *boolean ? "true" : "false");
                return {};
            }
        }
        std::println("{}", std::string_view{value.val().str, value.val().len});
    }
    return {};
}

} // namespace catalyst::introspect
