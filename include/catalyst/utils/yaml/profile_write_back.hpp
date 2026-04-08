#pragma once

#include <expected>
#include <string>

#include <catalyst/utils/yaml/load_profile_file.hpp>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/yaml.h>

namespace catalyst::utils::yaml {
std::expected<void, std::string> profileWriteBack(const ProfileFile &profile_file);
}
