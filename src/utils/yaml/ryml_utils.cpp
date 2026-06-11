#include "catalyst/utils/yaml/ryml_utils.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <format>
#include <fstream>
#include <sstream>

#include <c4/charconv.hpp>

namespace catalyst::utils::yaml {

namespace {

ryml::csubstr toSubstr(std::string_view sv) {
    return {sv.data(), sv.size()};
}

// Returns the node's scalar value, or nullopt if the node is not a readable
// non-null scalar. Every getter funnels through here so the yaml-cpp parity
// rules (see header) live in one place.
std::optional<ryml::csubstr> scalarVal(ryml::ConstNodeRef node) {
    if (!node.readable() || !node.has_val())
        return std::nullopt;
    if (node.val_is_null())
        return std::nullopt;
    return node.val();
}

} // namespace

std::expected<ryml::Tree, std::string> parseYaml(std::string_view contents, std::string_view filename) {
    try {
        return ryml::parse_in_arena(toSubstr(filename), toSubstr(contents));
    } catch (const std::exception &err) {
        return std::unexpected(std::format("Failed to parse YAML file: {}", err.what()));
    }
}

std::expected<ryml::Tree, std::string> loadFile(const std::filesystem::path &path) {
    std::ifstream in{path, std::ios::binary};
    if (!in)
        return std::unexpected(std::format("Failed to open YAML file: {}", path.string()));

    std::ostringstream contents;
    contents << in.rdbuf();
    if (in.bad())
        return std::unexpected(std::format("Failed to read YAML file: {}", path.string()));

    return parseYaml(contents.view(), path.string());
}

ryml::ConstNodeRef child(ryml::ConstNodeRef node, std::string_view key) {
    if (!node.readable() || !node.is_map())
        return {};
    ryml::id_type child_id = node.tree()->find_child(node.id(), toSubstr(key));
    if (child_id == ryml::NONE)
        return {};
    return {node.tree(), child_id};
}

std::optional<std::string> asString(ryml::ConstNodeRef node) {
    std::optional<ryml::csubstr> val = scalarVal(node);
    if (!val)
        return std::nullopt;
    return std::string{val->str, val->len};
}

std::optional<int> asInt(ryml::ConstNodeRef node) {
    std::optional<ryml::csubstr> val = scalarVal(node);
    int out = 0;
    if (!val || !c4::atoi(*val, &out))
        return std::nullopt;
    return out;
}

std::optional<bool> asBool(ryml::ConstNodeRef node) {
    std::optional<ryml::csubstr> val = scalarVal(node);
    if (!val)
        return std::nullopt;

    std::string lowered{val->str, val->len};
    std::ranges::transform(lowered, lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    constexpr std::array TRUE_NAMES{"y", "yes", "true", "on"};
    constexpr std::array FALSE_NAMES{"n", "no", "false", "off"};
    if (std::ranges::find(TRUE_NAMES, lowered) != TRUE_NAMES.end())
        return true;
    if (std::ranges::find(FALSE_NAMES, lowered) != FALSE_NAMES.end())
        return false;
    return std::nullopt;
}

std::optional<std::vector<std::string>> asStringVector(ryml::ConstNodeRef node) {
    if (!node.readable() || !node.is_seq())
        return std::nullopt;

    std::vector<std::string> out;
    for (ryml::ConstNodeRef child : node.children()) {
        std::optional<std::string> item = asString(child);
        if (!item)
            return std::nullopt;
        out.push_back(std::move(*item));
    }
    return out;
}

std::string emitYaml(const ryml::Tree &tree) {
    return ryml::emitrs_yaml<std::string>(tree);
}

} // namespace catalyst::utils::yaml
