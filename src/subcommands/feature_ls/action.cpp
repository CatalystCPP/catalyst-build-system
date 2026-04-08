#include <algorithm>
#include <expected>
#include <filesystem>
#include <optional>
#include <print>
#include <string>
#include <vector>

#include "catalyst/subcommands/feature_ls.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/yaml.h"

namespace fs = std::filesystem;

namespace {
void addCombinedProfiles(std::vector<std::string> &out_profiles);
void addIndividualProfiles(std::vector<std::string> &out_profiles);
void filterUnique(std::vector<std::string> &vec);
} // namespace

std::expected<void, std::string> catalyst::feature_ls::action([[maybe_unused]] const Parse &parse_res) {
    catalyst::logger.debug("feature-ls subcommand invoked.");
    std::vector<std::string> profiles;
    // load everything from CATALYST.yaml
    if (fs::exists("CATALYST.yaml")) {
        addCombinedProfiles(profiles);
    } else {
        catalyst::logger.debug("File: CATALYST.yaml not found");
    }
    addIndividualProfiles(profiles);
    filterUnique(profiles);

    std::vector<std::string> all_features;
    for (const auto &profile_name : profiles) {
        try {
            catalyst::utils::yaml::Configuration config({profile_name});
            const YAML::Node &root = config.getRoot();
            if (root["features"] && root["features"].IsMap()) {
                for (auto it = root["features"].begin(); it != root["features"].end(); ++it) {
                    all_features.push_back(it->first.as<std::string>());
                }
            }
        } catch (const std::exception &e) {
            catalyst::logger.debug("Failed to load profile '{}': {}", profile_name, e.what());
        }
    }

    filterUnique(all_features);

    catalyst::logger.debug("feature-ls subcommand finished successfully.");
    std::ranges::for_each(all_features, [](const auto &val) { std::println("{}", val); });
    return {};
}

namespace {
void addCombinedProfiles(std::vector<std::string> &out_profiles) {
    try {
        YAML::Node profiles = YAML::LoadFile("CATALYST.yaml");
        for (auto it : profiles) {
            out_profiles.push_back(it.first.as<std::string>());
        }
    } catch (...) {
    }
}

void addIndividualProfiles(std::vector<std::string> &out_profiles) {
    const std::string prefix = "catalyst_";
    const std::string suffix = ".yaml";

    for (const auto &entry : fs::directory_iterator(fs::current_path())) {
        if (!entry.is_regular_file())
            continue;

        auto filename = entry.path().filename().string();

        std::optional<std::string> profile_name{std::nullopt};
        if (filename == "catalyst.yaml") {
            profile_name = "common";
        } else if (filename.starts_with(prefix) && filename.ends_with(suffix) &&
                   filename.size() > prefix.size() + suffix.size()) {
            profile_name = filename.substr(prefix.size(), filename.size() - prefix.size() - suffix.size());
        }
        if (profile_name) {
            catalyst::logger.debug("Found profile: {} in {}", *profile_name, filename);
            out_profiles.push_back(*profile_name);
        }
    }
}

void filterUnique(std::vector<std::string> &vec) {
    std::ranges::sort(vec);
    const auto ret = std::ranges::unique(vec);
    vec.erase(ret.begin(), ret.end());
}
} // namespace
