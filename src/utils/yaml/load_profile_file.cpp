#include "catalyst/utils/yaml/load_profile_file.hpp"

#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <utility>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::utils::yaml {

namespace {

namespace fs = std::filesystem;

// An empty or null document root (e.g. an empty file) becomes an empty map so
// callers can add keys to it, like yaml-cpp's auto-vivification allowed.
void ensureMapRoot(ryml::Tree &tree) {
    ryml::id_type root_id = tree.root_id();
    if (tree.is_map(root_id) || tree.is_seq(root_id))
        return;
    if (tree.has_val(root_id) && !tree.val_is_null(root_id))
        return; // a scalar document; leave it alone
    // change_type (not |=) because a null root carries the VAL flag, which
    // conflicts with MAP.
    tree.change_type(root_id, ryml::MAP);
}

auto loadFromCombined(const std::string &profile, const fs::path &combined_path, const fs::path &profile_path)
    -> Result<ProfileFile> {
    auto parsed = loadFile(combined_path);
    if (!parsed)
        return std::unexpected(parsed.error());
    ryml::Tree tree = std::move(*parsed);
    ensureMapRoot(tree);
    ryml::NodeRef root = tree.rootref();

    ryml::id_type profile_id = ryml::NONE;
    if (fs::exists(profile_path)) {
        catalyst::logger.warn("Profile: {} was moved into CATALYST.yaml", profile);
        auto legacy = loadFile(profile_path);
        if (!legacy)
            return std::unexpected(legacy.error());
        // The isolated file's content replaces any same-named profile.
        // DON'T DELETE THE FILE, just ignore it. This way we can preserve user changes and avoid data loss.
        removeChild(root, profile);
        ryml::NodeRef profile_node = appendContentCopy(root, legacy->crootref());
        profile_node.set_key(tree.to_arena(toSubstr(profile)));
        if (!profile_node.is_map() && !profile_node.is_seq() && !profile_node.has_val())
            profile_node |= ryml::MAP;
        profile_id = profile_node.id();
    } else {
        profile_id = tree.find_child(tree.root_id(), toSubstr(profile));
        if (profile_id == ryml::NONE) {
            ryml::NodeRef profile_node = childOrCreate(root, profile);
            profile_node |= ryml::MAP;
            profile_id = profile_node.id();
        } else if (tree.has_val(profile_id) && tree.val_is_null(profile_id)) {
            // A bare `profile:` entry: turn the null into an empty map so the
            // profile node is mutable.
            tree.change_type(profile_id, ryml::MAP);
        }
    }

    catalyst::logger.debug("Combined profile file loaded successfully.");
    return ProfileFile{.tree = std::move(tree), .profile_id = profile_id, .path = combined_path};
}

auto loadFromIsolate(const std::string &profile, const fs::path &profile_path) -> Result<ProfileFile> {
    if (!fs::exists(profile_path))
        return std::unexpected(std::format("Profile file: {} for {} not found", profile_path.string(), profile));

    auto parsed = loadFile(profile_path);
    if (!parsed)
        return std::unexpected(parsed.error());
    ryml::Tree tree = std::move(*parsed);
    ensureMapRoot(tree);

    catalyst::logger.debug("Profile file loaded successfully.");
    ryml::id_type root_id = tree.root_id();
    return ProfileFile{.tree = std::move(tree), .profile_id = root_id, .path = profile_path};
}

} // anonymous namespace

auto loadProfileFile(const std::string &profile, const fs::path &root_dir) -> Result<ProfileFile> {
    catalyst::logger.debug("Loading profile file: {} from {}", profile, root_dir.string());

    const fs::path combined_path = root_dir / "CATALYST.yaml";
    fs::path profile_path = root_dir;
    if (profile == "common")
        profile_path /= "catalyst.yaml";
    else
        profile_path /= std::format("catalyst_{}.yaml", profile);

    catalyst::logger.debug("Profile path: {}", profile_path.string());

    if (fs::exists(combined_path))
        return loadFromCombined(profile, combined_path, profile_path);

    return loadFromIsolate(profile, profile_path);
}

} // namespace catalyst::utils::yaml
