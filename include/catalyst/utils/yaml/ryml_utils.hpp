#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ryml/ryml.hpp>
#include <ryml/ryml_std.hpp> // std::string interop for emit/serialize

namespace catalyst::utils::yaml {

/** @brief Converts a string_view to a ryml csubstr (no copy — same lifetime). */
inline ryml::csubstr toSubstr(std::string_view sv) {
    return {sv.data(), sv.size()};
}

/**
 * @brief Reads and parses a YAML file, replacing YAML::LoadFile.
 *
 * The file contents are copied into the returned tree's arena, so the tree is
 * self-contained. Errors (unreadable file, parse failure) are returned as the
 * unexpected value; this relies on the throwing error handler from
 * ryml_init.hpp being installed (done in main), otherwise rapidyaml aborts on
 * parse errors instead.
 */
std::expected<ryml::Tree, std::string> loadFile(const std::filesystem::path &path);

/**
 * @brief Parses YAML from an in-memory string into a self-contained tree.
 *
 * @param filename Used in parse error messages only.
 */
std::expected<ryml::Tree, std::string> parseYaml(std::string_view contents, std::string_view filename = "<string>");

/**
 * @brief Safe child lookup: returns an invalid (empty) ref if @p node is not
 * a readable map or has no child @p key.
 *
 * Use this instead of ConstNodeRef::operator[] whenever the key may be
 * absent: in ryml 0.9, operator[] on a missing key fires an assert (throws
 * via our error handler) in builds without NDEBUG, but silently returns an
 * invalid ref in release builds. This helper behaves the same way in every
 * build type, and its result is safe to pass to the getters below.
 */
ryml::ConstNodeRef child(ryml::ConstNodeRef node, std::string_view key);

/**
 * @name Checked scalar getters
 *
 * Replacements for yaml-cpp's throwing .as<T>() calls. All return nullopt
 * for nodes that are invalid, non-scalar (maps/sequences), null, or whose
 * scalar does not convert to T. Mirrors yaml-cpp semantics: an unquoted
 * empty/`~`/`null` value is null (nullopt), a quoted empty string is "".
 * @{
 */
std::optional<std::string> asString(ryml::ConstNodeRef node);
std::optional<int> asInt(ryml::ConstNodeRef node);
/** Accepts yaml-cpp's flexible booleans (y/yes/true/on, n/no/false/off), case-insensitively. */
std::optional<bool> asBool(ryml::ConstNodeRef node);
/** Requires a sequence whose children are all non-null scalars, like yaml-cpp's as<std::vector<std::string>>(). */
std::optional<std::vector<std::string>> asStringVector(ryml::ConstNodeRef node);
/** @} */

/**
 * @name Mutation helpers
 *
 * rapidyaml's own cross-tree copies (Tree::duplicate, Tree::merge_with) copy
 * raw string pointers, leaving the destination tree pointing into the source
 * tree's buffer — a dangling reference once the source is destroyed. The
 * helpers here re-anchor every copied key/value into the destination tree's
 * arena, so the result is self-contained. (Tags and anchors are not
 * re-anchored; catalyst profiles don't use them.)
 * @{
 */

/**
 * @brief Returns the map child @p key of @p parent, creating it (with the key
 * copied into the arena) if absent. Materializes @p parent as a map if it has
 * no type yet.
 */
ryml::NodeRef childOrCreate(ryml::NodeRef parent, std::string_view key);

/** @brief Removes the child @p key of @p parent (and its subtree), if present. */
void removeChild(ryml::NodeRef parent, std::string_view key);

/**
 * @brief Appends a self-contained copy of @p src (key, value, children, and
 * type flags) as the last child of @p parent. @p src may belong to another
 * tree; it must not be its tree's root. @p parent must be a container.
 * @return the copy.
 */
ryml::NodeRef appendCopy(ryml::NodeRef parent, ryml::ConstNodeRef src);

/**
 * @brief Like appendCopy, but copies only @p src's content (value or
 * children), not its key — for wrapping a keyed node into a sequence or
 * copying a whole document under a new key. @p src may be its tree's root.
 * @return the new keyless child.
 */
ryml::NodeRef appendContentCopy(ryml::NodeRef parent, ryml::ConstNodeRef src);
/** @} */

/**
 * @brief Emits the whole tree as YAML text, replacing YAML::Emitter.
 */
std::string emitYaml(const ryml::Tree &tree);

/** @brief Emits the subtree rooted at @p node as YAML text. */
std::string emitYaml(ryml::ConstNodeRef node);

} // namespace catalyst::utils::yaml
