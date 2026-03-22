#include "catalyst/utils/yaml/configuration.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "catalyst/utils/log/log.hpp"

#include "yaml-cpp/node/node.h"
#include "yaml-cpp/node/parse.h"

using catalyst::LogLevel;
using catalyst::utils::yaml::Configuration;
namespace fs = std::filesystem;

namespace {

YAML::Node getDefaultConfiguration() {
    YAML::Node root;
    root["meta"]["min_ver"] = "0.0.1";
    root["meta"]["generator"] = "cbe";
    root["manifest"]["name"] = "name";
    root["manifest"]["type"] = "BINARY";
    root["manifest"]["version"] = "0.0.1";
    root["manifest"]["provides"] = "";
    root["manifest"]["tooling"]["CC"] = "clang";
    root["manifest"]["tooling"]["CXX"] = "clang++";
    root["manifest"]["tooling"]["FMT"] = "clang-format";
    root["manifest"]["tooling"]["LINTER"] = "clang-tidy";
    root["manifest"]["tooling"]["CCFLAGS"] = "";
    root["manifest"]["tooling"]["CXXFLAGS"] = "";
    root["manifest"]["tooling"]["LDFLAGS"] = "";
    root["manifest"]["tooling"]["doc"]["engine"] = "doxygen";
    root["manifest"]["tooling"]["doc"]["config"] = "Doxyfile";
    root["manifest"]["tooling"]["doc"]["out_dir"] = "docs/";
    root["manifest"]["dirs"]["include"] = std::vector<std::string>{};
    root["manifest"]["dirs"]["source"] = std::vector<std::string>{};
    root["manifest"]["dirs"]["build"] = "";
    root["dependencies"] = std::vector<YAML::Node>{};
    root["features"] = YAML::Node(YAML::NodeType::Map);
    return root;
}

std::string verMax(std::string s1, std::string s2) {
    catalyst::logger.debug("Comparing versions: {} and {}", s1, s2);
    auto split_ver = [](const std::string &ver) {
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

    std::vector<int> v1 = split_ver(s1);
    std::vector<int> v2 = split_ver(s2);

    for (size_t i = 0, lim = std::min(v1.size(), v2.size()); i < lim; ++i) {
        if (v1[i] > v2[i]) {
            catalyst::logger.debug("Version {} is greater.", s1);
            return s1;
        }
        if (v1[i] < v2[i]) {
            catalyst::logger.debug("Version {} is greater.", s2);
            return s2;
        }
    }

    catalyst::logger.debug("Versions are equal, returning {}.", s1);
    return s1;
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

std::optional<YAML::Node> traverse(const std::string &key, const YAML::Node &root) {
    std::vector<std::string> segments = splitPath(key);
    YAML::Node current;
    current.reset(root);

    for (const auto &s : segments) {
        if (!current[s]) {
            return std::nullopt;
        }
        current.reset(current[s]);
    }
    return current;
}

void validateProfileKeys(const YAML::Node &profile, const std::string &profile_name) {
    if (!profile.IsMap())
        return;

    auto check_keys =
        [&](const YAML::Node &node, const std::vector<std::string> &allowed_keys, const std::string &path) {
            if (!node.IsMap())
                return;
            for (auto it = node.begin(); it != node.end(); ++it) {
                auto key = it->first.as<std::string>();
                if (std::ranges::find(allowed_keys, key) == allowed_keys.end()) {
                    catalyst::logger.warn("Invalid key '{}' found at '{}' in profile '{}'.", key, path, profile_name);
                    throw std::exception();
                }
            }
        };

    check_keys(profile, {"meta", "manifest", "dependencies", "features", "hooks"}, "root");
    if (profile["meta"]) {
        check_keys(profile["meta"], {"min_ver", "generator"}, "meta");
    }
    if (profile["manifest"]) {
        check_keys(
            profile["manifest"], {"name", "type", "version", "provides", "tooling", "dirs", "description"}, "manifest");
        if (profile["manifest"]["tooling"]) {
            check_keys(profile["manifest"]["tooling"],
                       {"CC", "CXX", "FMT", "LINTER", "CCFLAGS", "CXXFLAGS", "LDFLAGS", "doc"},
                       "manifest.tooling");
            if (profile["manifest"]["tooling"]["doc"]) {
                check_keys(
                    profile["manifest"]["tooling"]["doc"], {"engine", "config", "out_dir"}, "manifest.tooling.doc");
            }
        }
        if (profile["manifest"]["dirs"]) {
            check_keys(profile["manifest"]["dirs"], {"include", "source", "build"}, "manifest.dirs");
        }
    }
    if (profile["dependencies"] && profile["dependencies"].IsSequence()) {
        for (const auto &dep : profile["dependencies"]) {
            check_keys(dep,
                       {"name",
                        "source",
                        "version",
                        "triplet",
                        "using",
                        "linkage",
                        "path",
                        "url",
                        "profiles",
                        "include",
                        "lib",
                        "hash"},
                       "dependencies[]");
        }
    }
}

void merge_helper(YAML::Node &composite, const std::string &new_profile_name, const YAML::Node &new_profile) {
    validateProfileKeys(new_profile, new_profile_name);

    YAML::Node defaults = getDefaultConfiguration();

    auto check_conflict = [&](const std::string &dotpath, const std::string &incoming_val) {
        try {
            auto resolve = [](YAML::Node node, const std::string &path) -> std::string {
                for (const auto &seg : splitPath(path))
                    node = node[seg];
                return node.as<std::string>();
            };
            std::string current_val = resolve(YAML::Clone(composite), dotpath);
            std::string default_val = resolve(YAML::Clone(defaults), dotpath);

            if (current_val != incoming_val && current_val != default_val) {
                catalyst::logger.warn(
                    "Profile '{}' overrides '{}': '{}' -> '{}'", new_profile_name, dotpath, current_val, incoming_val);
            }
        } catch (...) {
            catalyst::logger.debug("Could not check conflict for '{}'", dotpath);
        }
    };

    auto merge_scalar =
        [&](YAML::Node dst_parent, const std::string &key, YAML::Node src_parent, const std::string &dotpath) {
            if (!src_parent[key].IsDefined())
                return;
            if (src_parent[key].IsNull()) {
                dst_parent.remove(key);
            } else {
                check_conflict(dotpath, src_parent[key].as<std::string>());
                dst_parent[key] = src_parent[key];
            }
        };

    auto merge_scalar_validated = [&](YAML::Node dst_parent,
                                      const std::string &key,
                                      YAML::Node src_parent,
                                      const std::string &dotpath,
                                      const std::function<bool(const std::string &)> &validator,
                                      const std::string &fallback_on_null = "") {
        if (!src_parent[key].IsDefined())
            return;
        if (src_parent[key].IsNull()) {
            if (!fallback_on_null.empty())
                dst_parent[key] = fallback_on_null;
            else
                dst_parent.remove(key);
        } else {
            auto val = src_parent[key].as<std::string>();
            if (validator(val)) {
                check_conflict(dotpath, val);
                dst_parent[key] = val;
            } else {
                catalyst::logger.warn(
                    "Invalid value '{}' for '{}' in profile '{}'. Ignoring.", val, dotpath, new_profile_name);
            }
        }
    };

    auto merge_sequence = [&](YAML::Node dst_parent, const std::string &key, YAML::Node src_parent) {
        if (!src_parent[key].IsDefined())
            return;
        if (src_parent[key].IsNull()) {
            dst_parent.remove(key);
        } else if (src_parent[key].IsSequence()) {
            for (const auto &item : src_parent[key])
                dst_parent[key].push_back(item);
        }
    };

    auto merge_section = [&](YAML::Node dst_parent,
                             const std::string &key,
                             YAML::Node src_parent,
                             const std::function<void(YAML::Node, YAML::Node)> &body) {
        if (!src_parent[key].IsDefined())
            return;
        if (src_parent[key].IsNull()) {
            dst_parent.remove(key);
        } else {
            body(dst_parent[key], src_parent[key]);
        }
    };

    merge_section(composite, "meta", new_profile, [&](YAML::Node dst, YAML::Node src) {
        if (src["min_ver"].IsDefined()) {
            if (src["min_ver"].IsNull()) {
                dst.remove("min_ver");
            } else {
                dst["min_ver"] = verMax(dst["min_ver"].as<std::string>(), src["min_ver"].as<std::string>());
            }
        }

        merge_scalar_validated(
            dst,
            "generator",
            src,
            "meta.generator",
            [](const std::string &v) { return v == "ninja" || v == "cbe"; },
            /*fallback_on_null=*/"cbe");
    });

    merge_section(composite, "manifest", new_profile, [&](YAML::Node dst, YAML::Node src) {
        for (const auto &key : {"name", "type", "version", "provides"})
            merge_scalar(dst, key, src, std::string("manifest.") + key);

        merge_section(dst, "tooling", src, [&](YAML::Node tdst, YAML::Node tsrc) {
            for (const auto &key : {"CC", "CXX", "FMT", "LINTER", "CCFLAGS", "CXXFLAGS", "LDFLAGS"})
                merge_scalar(tdst, key, tsrc, std::string("manifest.tooling.") + key);

            merge_section(tdst, "doc", tsrc, [&](YAML::Node docdst, YAML::Node docsrc) {
                for (const auto &key : {"engine", "config", "out_dir"})
                    merge_scalar(docdst, key, docsrc, std::string("manifest.tooling.doc.") + key);
            });
        });

        merge_section(dst, "dirs", src, [&](YAML::Node ddst, YAML::Node dsrc) {
            merge_sequence(ddst, "include", dsrc);
            merge_sequence(ddst, "source", dsrc);
            merge_scalar(ddst, "build", dsrc, "manifest.dirs.build");
        });
    });

    merge_section(composite, "features", new_profile, [&](YAML::Node dst, YAML::Node src) {
        if (src.IsMap()) {
            for (const auto &it : src) {
                dst[it.first.as<std::string>()] = it.second;
            }
        } else if (src.IsSequence()) {
            for (const auto &item : src) {
                if (item.IsMap()) {
                    for (const auto &it : item) {
                        dst[it.first.as<std::string>()] = it.second;
                    }
                }
            }
        }
    });
    merge_sequence(composite, "dependencies", new_profile);

    merge_section(composite, "hooks", new_profile, [&](YAML::Node dst, YAML::Node src) {
        for (const auto &hook : src) {
            auto name = hook.first.as<std::string>();
            if (hook.second.IsNull()) {
                dst.remove(name);
            } else if (hook.second.IsSequence()) {
                for (const auto &item : hook.second)
                    dst[name].push_back(item);
            } else if (hook.second.IsScalar()) {
                dst[name].push_back(hook.second);
            }
        }
    });
}

void merge(YAML::Node &composite, const std::string &profile_name, const fs::path &root_dir) {
    if (fs::exists(root_dir / "CATALYST.yaml")) {
        if (YAML::Node catalyst_yaml = YAML::LoadFile(root_dir / "CATALYST.yaml"); catalyst_yaml[profile_name]) {
            catalyst::logger.debug("Found profile '{}' in CATALYST.yaml", profile_name);
            merge_helper(composite, profile_name, catalyst_yaml[profile_name]);
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
        throw std::exception();
    }
    merge_helper(composite, profile_name, YAML::LoadFile(profile_path));
}

} // namespace

Configuration::Configuration(const std::vector<std::string> &profiles, const std::filesystem::path &root_dir) {
    std::vector profile_names = profiles;
    catalyst::logger.debug("Composing profiles: {}.", profile_names);

    root = getDefaultConfiguration();

    // NOTE: PERF: This is possibly more performant than creating a temporary std::unordered_set
    for (size_t ii = 0; ii < profiles.size(); ++ii) {
        for (size_t jj = 0; jj < ii; ++jj) {
            if (profiles[jj] == profiles[ii]) {
                catalyst::logger.error(
                    "Duplicate profiles: {0} at index {1} and {0} and index {2}", profiles[ii], jj, ii);
                throw std::exception();
            }
        }
    }

    for (const auto &profile_name : profile_names) {
        merge(root, profile_name, root_dir);
    }

    catalyst::logger.debug("Profile composition finished.");
}

bool Configuration::has(const std::string &key) const {
    std::vector<std::string> segments = splitPath(key);
    YAML::Node current = YAML::Clone(root);

    for (const auto &segment : segments) {
        if (!current[segment]) {
            return false;
        }
        current = current[segment];
    }
    return true;
}

std::optional<std::string> Configuration::getString(const std::string &key) const {
    std::optional<YAML::Node> res = traverse(key, root);
    if (!res) {
        return std::nullopt;
    }

    try {
        return res.value().as<std::string>();
    } catch (const YAML::Exception &e) {
        catalyst::logger.error("YAML Exception in getString for key {}: {}", key, e.what());
        return std::nullopt;
    }
}

std::optional<int> Configuration::getInt(const std::string &key) const {
    std::optional<YAML::Node> res = traverse(key, root);
    if (!res) {
        return std::nullopt;
    }

    try {
        return res.value().as<int>();
    } catch (const YAML::Exception &) {
        return std::nullopt;
    }
}

std::optional<bool> Configuration::getBool(const std::string &key) const {
    std::optional<YAML::Node> res = traverse(key, root);
    if (!res) {
        return std::nullopt;
    }

    try {
        return res.value().as<bool>();
    } catch (const YAML::Exception &) {
        return std::nullopt;
    }
}

std::optional<std::vector<std::string>> Configuration::getStringVector(const std::string &key) const {
    std::optional<YAML::Node> res = traverse(key, root);
    if (!res) {
        return std::nullopt;
    }

    try {
        return res.value().as<std::vector<std::string>>();
    } catch (const YAML::Exception &) {
        return std::nullopt;
    }
}
