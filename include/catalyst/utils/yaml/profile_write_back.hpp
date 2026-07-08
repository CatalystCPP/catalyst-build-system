#pragma once

#include <expected>
#include <string>

#include <catalyst/utils/yaml/load_profile_file.hpp>

#include "catalyst/utils/result.hpp"

namespace catalyst::utils::yaml {

/// @brief Write profile to appropriate file
Result<void> profileWriteBack(const ProfileFile &profile_file);
}
