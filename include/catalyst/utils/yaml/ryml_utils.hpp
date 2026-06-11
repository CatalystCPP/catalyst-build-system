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
 * @brief Emits the whole tree as YAML text, replacing YAML::Emitter.
 */
std::string emitYaml(const ryml::Tree &tree);

} // namespace catalyst::utils::yaml
