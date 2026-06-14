#pragma once
#include <expected>
#include "catalyst/utils/result.hpp"
#include <filesystem>
#include <string>

#include <catalyst/utils/yaml/ryml_utils.hpp>

namespace catalyst::utils::yaml {

/**
 * A profile file loaded for mutation and write-back. The tree owns the whole
 * document; the profile's node is stored as an id (not a NodeRef) so that
 * moving the struct cannot dangle a Tree pointer. For combined CATALYST.yaml
 * files the profile node is the keyed map under the profile name; for
 * isolated catalyst[_<profile>].yaml files it is the document root.
 */
struct ProfileFile {
    ryml::Tree tree;
    ryml::id_type profile_id;
    std::filesystem::path path;

    ryml::NodeRef root() {
        return tree.rootref();
    }
    ryml::NodeRef profile() {
        return {&tree, profile_id};
    }
};

Result<ProfileFile>
loadProfileFile(const std::string &profile, const std::filesystem::path &root_dir = std::filesystem::current_path());
} // namespace catalyst::utils::yaml
