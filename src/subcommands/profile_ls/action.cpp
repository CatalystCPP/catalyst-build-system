#include <algorithm>
#include <expected>
#include <filesystem>
#include <optional>
#include <print>
#include <string>
#include <vector>

#include "catalyst/subcommands/profile_ls.hpp"
#include "catalyst/utils/log/log.hpp"

#include "yaml-cpp/yaml.h"

namespace fs = std::filesystem;

namespace {
void addCombinedProfiles(std::vector<std::string> &out_profiles);
void addIndividualProfiles(std::vector<std::string> &out_profiles);
void filterUnique(std::vector<std::string> &profiles);
} // namespace

std::expected<void, std::string> catalyst::profile_ls::action([[maybe_unused]] const Parse &parse_res) {
    catalyst::logger.debug("profile-ls subcommand invoked.");
    std::vector<std::string> profiles;
    // load everything from CATALYST.yaml
    if (fs::exists("CATALYST.yaml")) {
        addCombinedProfiles(profiles);
    } else
        catalyst::logger.debug("File: CATALYST.yaml not fond");
    addIndividualProfiles(profiles);
    filterUnique(profiles);
    // load everything from catalyst_*.yaml
    catalyst::logger.debug("profile-ls subcommand finished successfully.");
    std::ranges::for_each(profiles, [](const auto &val) { std::println("{}", val); });
    return {};
}

namespace {
void addCombinedProfiles(std::vector<std::string> &out_profiles) {
    YAML::Node profiles = YAML::LoadFile("CATALYST.yaml");
    for (auto it : profiles) {
        out_profiles.push_back(it.first.as<std::string>());
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

void filterUnique(std::vector<std::string> &profiles) {
    std::ranges::sort(profiles);
    const auto ret = std::ranges::unique(profiles);
    profiles.erase(ret.begin(), ret.end());
}
} // namespace
