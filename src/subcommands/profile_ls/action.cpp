#include <algorithm>
#include <expected>
#include <filesystem>
#include <optional>
#include <print>
#include <string>
#include <vector>

#include "catalyst/subcommands/profile_ls.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace fs = std::filesystem;

namespace {
[[nodiscard]] catalyst::Result<std::vector<std::string>> loadCombinedProfiles();
void addIndividualProfiles(std::vector<std::string> &out_profiles);
void filterUnique(std::vector<std::string> &profiles);
} // namespace

catalyst::Result<void> catalyst::profile_ls::action(const Parse &parse_res) {
    catalyst::logger.debug("profile-ls subcommand invoked.");
    std::vector<std::string> profiles;
    // load everything from CATALYST.yaml
    if (fs::exists("CATALYST.yaml")) {
        auto combined = loadCombinedProfiles();
        if (!combined)
            return std::unexpected(combined.error());
        profiles = std::move(*combined);
    } else
        catalyst::logger.debug("File: CATALYST.yaml not found");
    addIndividualProfiles(profiles);
    filterUnique(profiles);
    // load everything from catalyst_*.yaml
    catalyst::logger.debug("profile-ls subcommand finished successfully.");
    if (parse_res.json) {
        ryml::Tree tree;
        auto root = tree.rootref();
        root |= ryml::SEQ;
        for (const auto &profile : profiles) {
            auto value = root.append_child();
            value << profile;
            value |= ryml::VALQUO;
        }
        std::println("{}", ryml::emitrs_json<std::string>(tree));
    } else {
        std::ranges::for_each(profiles, [](const auto &val) { std::println("{}", val); });
    }
    return {};
}

namespace {
catalyst::Result<std::vector<std::string>> loadCombinedProfiles() {
    auto tree = catalyst::utils::yaml::loadFile("CATALYST.yaml");
    if (!tree)
        return std::unexpected(tree.error());
    ryml::ConstNodeRef root = tree->crootref();
    if (!root.is_map())
        return std::unexpected("CATALYST.yaml must contain a mapping of profile names to configurations");
    std::vector<std::string> profiles;
    for (ryml::ConstNodeRef profile : root.children()) {
        if (profile.has_key())
            profiles.emplace_back(profile.key().str, profile.key().len);
    }
    return profiles;
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
        } else if (filename.starts_with(prefix) && filename.ends_with(suffix)
                   && filename.size() > prefix.size() + suffix.size()) {
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
