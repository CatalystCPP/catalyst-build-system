#include "catalyst/utils/yaml/configuration.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

using catalyst::utils::yaml::appendContentCopy;
using catalyst::utils::yaml::appendCopy;
using catalyst::utils::yaml::asBool;
using catalyst::utils::yaml::asInt;
using catalyst::utils::yaml::asString;
using catalyst::utils::yaml::asStringVector;
using catalyst::utils::yaml::child;
using catalyst::utils::yaml::childOrCreate;
using catalyst::utils::yaml::Configuration;
using catalyst::utils::yaml::emitYaml;
using catalyst::utils::yaml::removeChild;
using catalyst::utils::yaml::toSubstr;
namespace fs = std::filesystem;

namespace {

// The default configuration every composition starts from. Parsing a literal
// (instead of building the tree programmatically) keeps the quoted-empty
// scalars ('' is an empty string, not null) and saves rapidyaml's verbose
// node-creation idioms.
constexpr std::string_view DEFAULT_CONFIGURATION_YAML = R"(meta:
  min_ver: 0.0.1
  generator: cob
manifest:
  name: name
  type: BINARY
  version: 0.0.1
  provides: ''
  tooling:
    FMT: clang-format
    LINTER: clang-tidy
    doc:
      engine: doxygen
      config: Doxyfile
      out_dir: docs/
  dirs:
    include: []
    source: []
    build: ''
dependencies: []
features: {}
)";

const ryml::Tree &getDefaultConfiguration() {
    static const ryml::Tree default_tree = []() {
        auto tree = catalyst::utils::yaml::parseYaml(DEFAULT_CONFIGURATION_YAML, "<defaults>");
        if (!tree)
            throw std::runtime_error(tree.error());
        return std::move(*tree);
    }();
    return default_tree;
}

std::string verMax(const std::string &version_1, const std::string &version_2) {
    catalyst::logger.debug("Comparing versions: {} and {}", version_1, version_2);
    auto split_ver = [](const std::string &ver) -> std::vector<int> {
        std::vector<int> parts;
        std::istringstream iss(ver);
        for (std::string token; std::getline(iss, token, '.');) {
            try {
                parts.push_back(std::stoi(token));
            } catch (...) {
                parts.push_back(0);
            }
        }
        while (parts.size() < 3)
            parts.push_back(0);
        return parts;
    };

    std::vector<int> v1 = split_ver(version_1);
    std::vector<int> v2 = split_ver(version_2);

    for (size_t i = 0, lim = std::min(v1.size(), v2.size()); i < lim; ++i) {
        if (v1[i] > v2[i]) {
            catalyst::logger.debug("Version {} is greater.", version_1);
            return version_1;
        }
        if (v1[i] < v2[i]) {
            catalyst::logger.debug("Version {} is greater.", version_2);
            return version_2;
        }
    }

    catalyst::logger.debug("Versions are equal, returning {}.", version_1);
    return version_1;
}

std::vector<std::string> splitPath(const std::string &key) {
    std::stringstream ss(key);
    std::string segment;
    std::vector<std::string> segments;
    while (std::getline(ss, segment, '.')) {
        segments.push_back(segment);
    }
    return segments;
}

std::optional<ryml::ConstNodeRef> traverse(const std::string &key, ryml::ConstNodeRef root) {
    ryml::ConstNodeRef current = root;
    for (const auto &s : splitPath(key)) {
        current = child(current, s);
        if (!current.readable())
            return std::nullopt;
    }
    return current;
}

// True for a key whose value is null (`key:`, `key: ~`, `key: null`) — the
// "remove this key" marker in profile merging. Containers are not null.
bool isNullValue(ryml::ConstNodeRef node) {
    return node.readable() && node.has_val() && node.val_is_null();
}

// Sets (creating or overwriting) the scalar child `key` of `parent`, copying
// both strings into the tree's arena.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void setScalarChild(ryml::NodeRef parent, std::string_view key, std::string_view value) {
    ryml::NodeRef node = childOrCreate(parent, key);
    node.set_val(parent.tree()->to_arena(toSubstr(value)));
}

void validateProfileKeys(ryml::ConstNodeRef profile, const std::string &profile_name) {
    if (!profile.readable() || !profile.is_map())
        return;

    auto check_keys =
        [&](ryml::ConstNodeRef node, const std::vector<std::string_view> &allowed_keys, const std::string &path) {
            if (!node.readable() || !node.is_map())
                return;
            for (ryml::ConstNodeRef item : node.children()) {
                if (!item.has_key())
                    continue;
                std::string key{item.key().str, item.key().len};
                if (std::ranges::find(allowed_keys, key) == allowed_keys.end()) {
                    catalyst::logger.warn("Invalid key '{}' found at '{}' in profile '{}'.", key, path, profile_name);
                    throw std::runtime_error(
                        std::format("Invalid key '{}' found at '{}' in profile '{}'.", key, path, profile_name));
                }
            }
        };

    check_keys(profile, {"meta", "manifest", "dependencies", "features", "hooks"}, "root");
    check_keys(child(profile, "meta"), {"min_ver", "generator"}, "meta");

    ryml::ConstNodeRef manifest = child(profile, "manifest");
    check_keys(manifest,
               {"name",
                "type",
                "version",
                "provides",
                "toolchain",
                "tooling",
                "dirs",
                "description",
                "author",
                "maintainer",
                "vendor",
                "license_file",
                "readme_file"},
               "manifest");
    ryml::ConstNodeRef tooling = child(manifest, "tooling");
    // manifest.tooling.{CC,CXX,CCFLAGS,CXXFLAGS,LDFLAGS} were removed in 1.7.0 in
    // favour of toolchain files. Point upgraders at the replacement instead of the
    // generic "invalid key" error.
    static constexpr std::array REMOVED_TOOLING = {
        std::pair{"CC", "toolchain compiler.c.executable"},
        std::pair{"CXX", "toolchain compiler.cxx.executable"},
        std::pair{"CCFLAGS", "toolchain compiler.c.flags"},
        std::pair{"CXXFLAGS", "toolchain compiler.cxx.flags"},
        std::pair{"LDFLAGS", "toolchain linker.flags"},
    };
    if (tooling.readable() && tooling.is_map()) {
        for (const auto &[key, replacement] : REMOVED_TOOLING) {
            if (child(tooling, key).readable()) {
                throw std::runtime_error(
                    std::format("manifest.tooling.{} was removed in 1.7.0 (profile '{}'). Define it via a "
                                "toolchain file ({}) instead. See "
                                "https://catalystcpp.github.io/catalyst-build-system/concepts/toolchains/",
                                key,
                                profile_name,
                                replacement));
            }
        }
    }
    check_keys(tooling, {"CC_LAUNCHER", "CXX_LAUNCHER", "FMT", "LINTER", "doc"}, "manifest.tooling");
    check_keys(child(tooling, "doc"), {"engine", "config", "out_dir"}, "manifest.tooling.doc");
    check_keys(child(manifest, "dirs"), {"include", "source", "build"}, "manifest.dirs");

    if (ryml::ConstNodeRef deps = child(profile, "dependencies"); deps.readable() && deps.is_seq()) {
        for (ryml::ConstNodeRef dep : deps.children()) {
            check_keys(dep,
                       {"name",
                        "source",
                        "version",
                        "triplet",
                        "using",
                        "linkage",
                        "transitive",
                        "path",
                        "url",
                        "profiles",
                        "include",
                        "lib",
                        "hash",
                        "command",
                        "script"},
                       "dependencies[]");
        }
    }
    check_keys(child(profile, "hooks"),
               {"pre-clean",
                "post-clean",
                "pre-run",
                "post-run",
                "pre-test",
                "post-test",
                "pre-bench",
                "post-bench",
                "pre-pack",
                "post-pack",
                "pre-build",
                "post-build",
                "on-build-failure",
                "pre-generate",
                "post-generate",
                "pre-fetch",
                "post-fetch"},
               "hooks");
}

// The single switchboard for catalyst's profile-merge semantics; splitting it
// would scatter the override/append/remove rules across functions.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void mergeHelper(ryml::Tree &composite, const std::string &new_profile_name, ryml::ConstNodeRef new_profile) {
    validateProfileKeys(new_profile, new_profile_name);

    ryml::ConstNodeRef defaults = getDefaultConfiguration().crootref();

    auto check_conflict = [&](const std::string &dotpath, const std::string &incoming_val) {
        auto current_node = traverse(dotpath, composite.crootref());
        auto default_node = traverse(dotpath, defaults);

        std::string current_val = current_node ? asString(*current_node).value_or("") : "";
        std::string default_val = default_node ? asString(*default_node).value_or("") : "";

        if (!current_val.empty() && current_val != incoming_val && current_val != default_val) {
            catalyst::logger.info("Profile '{}' overrides '{}'", new_profile_name, dotpath);
            catalyst::logger.debug("'{}': '{}' -> '{}'", dotpath, current_val, incoming_val);
        }
    };

    auto merge_scalar =
        [&](ryml::NodeRef dst_parent, std::string_view key, ryml::ConstNodeRef src_parent, const std::string &dotpath) {
            ryml::ConstNodeRef src_child = child(src_parent, key);
            if (!src_child.readable())
                return;
            if (isNullValue(src_child)) {
                removeChild(dst_parent, key);
            } else {
                check_conflict(dotpath, asString(src_child).value_or(""));
                // Replace wholesale (appendCopy keeps the quote flags, so a
                // quoted-empty '' survives as an empty string, not null).
                removeChild(dst_parent, key);
                appendCopy(dst_parent, src_child);
            }
        };

    auto merge_scalar_validated = [&](ryml::NodeRef dst_parent,
                                      std::string_view key,
                                      ryml::ConstNodeRef src_parent,
                                      const std::string &dotpath,
                                      const std::function<bool(const std::string &)> &validator,
                                      const std::string &fallback_on_null = "") {
        ryml::ConstNodeRef src_child = child(src_parent, key);
        if (!src_child.readable())
            return;
        if (isNullValue(src_child)) {
            if (!fallback_on_null.empty())
                setScalarChild(dst_parent, key, fallback_on_null);
            else
                removeChild(dst_parent, key);
        } else {
            auto val = asString(src_child).value_or("");
            if (validator(val)) {
                check_conflict(dotpath, val);
                setScalarChild(dst_parent, key, val);
            } else {
                catalyst::logger.warn(
                    "Invalid value '{}' for '{}' in profile '{}'. Ignoring.", val, dotpath, new_profile_name);
            }
        }
    };

    auto merge_sequence = [&](ryml::NodeRef dst_parent, std::string_view key, ryml::ConstNodeRef src_parent) {
        ryml::ConstNodeRef src_child = child(src_parent, key);
        if (!src_child.readable())
            return;
        if (isNullValue(src_child)) {
            removeChild(dst_parent, key);
        } else if (src_child.is_seq()) {
            ryml::NodeRef dst_seq = childOrCreate(dst_parent, key);
            dst_seq |= ryml::SEQ;
            for (ryml::ConstNodeRef item : src_child.children())
                appendCopy(dst_seq, item);
        }
    };

    auto merge_section = [&](ryml::NodeRef dst_parent,
                             std::string_view key,
                             ryml::ConstNodeRef src_parent,
                             const std::function<void(ryml::NodeRef, ryml::ConstNodeRef)> &body) {
        ryml::ConstNodeRef src_child = child(src_parent, key);
        if (!src_child.readable())
            return;
        if (isNullValue(src_child)) {
            removeChild(dst_parent, key);
        } else {
            ryml::NodeRef dst_child = childOrCreate(dst_parent, key);
            dst_child |= ryml::MAP;
            body(dst_child, src_child);
        }
    };

    ryml::NodeRef composite_root = composite.rootref();

    merge_section(composite_root, "meta", new_profile, [&](ryml::NodeRef dst, ryml::ConstNodeRef src) {
        if (ryml::ConstNodeRef src_min_ver = child(src, "min_ver"); src_min_ver.readable()) {
            if (isNullValue(src_min_ver)) {
                removeChild(dst, "min_ver");
            } else {
                std::string current = asString(child(dst, "min_ver")).value_or("");
                setScalarChild(dst, "min_ver", verMax(current, asString(src_min_ver).value_or("")));
            }
        }

        merge_scalar_validated(
            dst,
            "generator",
            src,
            "meta.generator",
            [](const std::string &v) { return v == "ninja" || v == "cob" || v == "gmake" || v == "make"; },
            /*fallback_on_null=*/"cob");
    });

    merge_section(composite_root, "manifest", new_profile, [&](ryml::NodeRef dst, ryml::ConstNodeRef src) {
        for (const auto &key : {"name",
                                "type",
                                "version",
                                "provides",
                                "toolchain",
                                "description",
                                "author",
                                "maintainer",
                                "vendor",
                                "license_file",
                                "readme_file"})
            merge_scalar(dst, key, src, std::string("manifest.") + key);

        merge_section(dst, "tooling", src, [&](ryml::NodeRef tdst, ryml::ConstNodeRef tsrc) {
            for (const auto &key : {"CC_LAUNCHER", "CXX_LAUNCHER", "FMT", "LINTER"})
                merge_scalar(tdst, key, tsrc, std::string("manifest.tooling.") + key);

            merge_section(tdst, "doc", tsrc, [&](ryml::NodeRef docdst, ryml::ConstNodeRef docsrc) {
                for (const auto &key : {"engine", "config", "out_dir"})
                    merge_scalar(docdst, key, docsrc, std::string("manifest.tooling.doc.") + key);
            });
        });

        merge_section(dst, "dirs", src, [&](ryml::NodeRef ddst, ryml::ConstNodeRef dsrc) {
            merge_sequence(ddst, "include", dsrc);
            merge_sequence(ddst, "source", dsrc);
            merge_scalar(ddst, "build", dsrc, "manifest.dirs.build");
        });
    });

    merge_section(composite_root, "features", new_profile, [&](ryml::NodeRef dst, ryml::ConstNodeRef src) {
        auto merge_feature_map = [&](ryml::ConstNodeRef map) {
            for (ryml::ConstNodeRef item : map.children()) {
                if (!item.has_key())
                    continue;
                removeChild(dst, std::string_view{item.key().str, item.key().len});
                appendCopy(dst, item);
            }
        };
        if (src.is_map()) {
            merge_feature_map(src);
        } else if (src.is_seq()) {
            for (ryml::ConstNodeRef item : src.children()) {
                if (item.is_map())
                    merge_feature_map(item);
            }
        }
    });
    merge_sequence(composite_root, "dependencies", new_profile);

    merge_section(composite_root, "hooks", new_profile, [&](ryml::NodeRef dst, ryml::ConstNodeRef src) {
        for (ryml::ConstNodeRef hook : src.children()) {
            if (!hook.has_key())
                continue;
            std::string_view name{hook.key().str, hook.key().len};
            if (isNullValue(hook)) {
                removeChild(dst, name);
            } else if (hook.is_seq()) {
                ryml::NodeRef dst_seq = childOrCreate(dst, name);
                dst_seq |= ryml::SEQ;
                for (ryml::ConstNodeRef item : hook.children())
                    appendCopy(dst_seq, item);
            } else if (hook.has_val() || hook.is_map()) {
                ryml::NodeRef dst_seq = childOrCreate(dst, name);
                dst_seq |= ryml::SEQ;
                // Copy the hook's content only: its key ("pre-build", ...)
                // must not follow it into the sequence item.
                appendContentCopy(dst_seq, hook);
            }
        }
    });
}

void merge(ryml::Tree &composite, const std::string &profile_name, const fs::path &root_dir) {
    if (fs::exists(root_dir / "CATALYST.yaml")) {
        auto catalyst_yaml = catalyst::utils::yaml::loadFile(root_dir / "CATALYST.yaml");
        if (!catalyst_yaml)
            throw std::runtime_error(catalyst_yaml.error());
        if (ryml::ConstNodeRef profile = child(catalyst_yaml->crootref(), profile_name); profile.readable()) {
            catalyst::logger.debug("Found profile '{}' in CATALYST.yaml", profile_name);
            mergeHelper(composite, profile_name, profile);
            return;
        }
    }

    // fallback
    fs::path profile_path = root_dir;
    if (profile_name == "common")
        profile_path /= "catalyst.yaml";
    else
        profile_path /= std::format("catalyst_{}.yaml", profile_name);

    if (!fs::exists(profile_path)) {
        catalyst::logger.error("Profile {} not found in {} or CATALYST.yaml", profile_name, profile_path.string());
        throw std::runtime_error(
            std::format("Profile {} not found in {} or CATALYST.yaml", profile_name, profile_path.string()));
    }
    auto profile_yaml = catalyst::utils::yaml::loadFile(profile_path);
    if (!profile_yaml)
        throw std::runtime_error(profile_yaml.error());
    mergeHelper(composite, profile_name, profile_yaml->crootref());
}

} // namespace

Configuration::Configuration(const std::vector<std::string> &profiles, const std::filesystem::path &root_dir)
    : composition(getDefaultConfiguration()) {
    std::vector<std::string> profile_names;
    profile_names.reserve(profiles.size());
    for (const auto &p : profiles) {
        if (!profile_names.empty() && profile_names.back() == p) {
            catalyst::logger.warn("Adjacent duplicate profile '{}' ignored during composition.", p);
        } else {
            profile_names.push_back(p);
        }
    }
    catalyst::logger.debug("Composing profiles: {}.", profile_names);

    for (const auto &profile_name : profile_names) {
        merge(composition, profile_name, root_dir);
    }

    this->profile_names = profile_names;
    catalyst::logger.debug("Profile composition finished.");
}

bool Configuration::has(const std::string &key) const {
    return traverse(key, composition.crootref()).has_value();
}

std::optional<std::string> Configuration::getString(const std::string &key) const {
    std::optional<ryml::ConstNodeRef> res = traverse(key, composition.crootref());
    if (!res) {
        return std::nullopt;
    }
    return asString(*res);
}

std::optional<int> Configuration::getInt(const std::string &key) const {
    std::optional<ryml::ConstNodeRef> res = traverse(key, composition.crootref());
    if (!res) {
        return std::nullopt;
    }
    return asInt(*res);
}

std::optional<bool> Configuration::getBool(const std::string &key) const {
    std::optional<ryml::ConstNodeRef> res = traverse(key, composition.crootref());
    if (!res) {
        return std::nullopt;
    }
    return asBool(*res);
}

std::optional<std::vector<std::string>> Configuration::getStringVector(const std::string &key) const {
    std::optional<ryml::ConstNodeRef> res = traverse(key, composition.crootref());
    if (!res) {
        return std::nullopt;
    }
    return asStringVector(*res);
}

std::filesystem::path Configuration::getBuildDir() const {
    std::string base = getString("manifest.dirs.build").value_or("build");
    if (base.empty())
        base = "build";
    return multiplexedBuildDir(base, profile_names);
}
