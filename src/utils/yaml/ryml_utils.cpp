#include "catalyst/utils/yaml/ryml_utils.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <format>
#include <fstream>
#include <sstream>

#include <c4/charconv.hpp>

#include "catalyst/utils/result.hpp"

namespace catalyst::utils::yaml {

namespace {

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

Result<ryml::Tree> parseYaml(std::string_view contents, std::string_view filename) {
    try {
        return ryml::parse_in_arena(toSubstr(filename), toSubstr(contents));
    } catch (const std::exception &err) {
        return std::unexpected(std::format("Failed to parse YAML file: {}", err.what()));
    }
}

Result<ryml::Tree> loadFile(const std::filesystem::path &path) {
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
    std::ranges::transform(
        lowered, lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

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
    for (ryml::ConstNodeRef node_child : node.children()) {
        std::optional<std::string> item = asString(node_child);
        if (!item)
            return std::nullopt;
        out.emplace_back(std::move(*item));
    }
    return out;
}

namespace {

// Copies every key/value string reachable from node into its own tree's
// arena. Run after a cross-tree Tree::duplicate*, whose copies still point
// into the source tree's buffer. Strings already in the arena are unaffected
// (arena growth relocates them automatically).
void reanchorStrings(ryml::NodeRef node) {
    ryml::Tree *tree = node.tree();
    if (node.has_key())
        node.set_key(tree->to_arena(node.key()));
    if (node.has_val())
        node.set_val(tree->to_arena(node.val()));
    for (ryml::NodeRef child_node : node.children())
        reanchorStrings(child_node);
}

} // namespace

ryml::NodeRef childOrCreate(ryml::NodeRef parent, std::string_view key) {
    if (!parent.is_map())
        parent |= ryml::MAP;
    ryml::Tree *tree = parent.tree();
    ryml::id_type child_id = tree->find_child(parent.id(), toSubstr(key));
    if (child_id != ryml::NONE)
        return {tree, child_id};
    ryml::NodeRef created = parent.append_child();
    created.set_key(tree->to_arena(toSubstr(key)));
    return created;
}

void removeChild(ryml::NodeRef parent, std::string_view key) {
    if (!parent.readable() || !parent.is_map())
        return;
    ryml::id_type child_id = parent.tree()->find_child(parent.id(), toSubstr(key));
    if (child_id != ryml::NONE)
        parent.tree()->remove(child_id);
}

ryml::NodeRef appendCopy(ryml::NodeRef parent, ryml::ConstNodeRef src) {
    ryml::Tree *tree = parent.tree();
    ryml::id_type copy_id = tree->duplicate(src.tree(), src.id(), parent.id(), tree->last_child(parent.id()));
    ryml::NodeRef copy{tree, copy_id};
    reanchorStrings(copy);
    return copy;
}

ryml::NodeRef appendContentCopy(ryml::NodeRef parent, ryml::ConstNodeRef src) {
    ryml::NodeRef copy = parent.append_child();
    parent.tree()->duplicate_contents(src.tree(), src.id(), copy.id());
    reanchorStrings(copy);
    return copy;
}

std::string emitYaml(const ryml::Tree &tree) {
    return ryml::emitrs_yaml<std::string>(tree);
}

std::string emitYaml(ryml::ConstNodeRef node) {
    return ryml::emitrs_yaml<std::string>(node);
}

} // namespace catalyst::utils::yaml
