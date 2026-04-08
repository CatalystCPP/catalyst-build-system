#pragma once
#include <expected>
#include <filesystem>
#include <string>

#include <yaml-cpp/yaml.h>

namespace catalyst::utils::yaml {

struct ProfileFile {
    YAML::Node root_node;
    YAML::Node profile_node;
    std::filesystem::path path;
};

std::expected<ProfileFile, std::string>
loadProfileFile(const std::string &profile, const std::filesystem::path &root_dir = std::filesystem::current_path());
} // namespace catalyst::utils::yaml
