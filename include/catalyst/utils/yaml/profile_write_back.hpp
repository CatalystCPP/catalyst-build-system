#pragma once

#include <expected>
#include "catalyst/utils/result.hpp"
#include <string>

#include <catalyst/utils/yaml/load_profile_file.hpp>

namespace catalyst::utils::yaml {
Result<void> profileWriteBack(const ProfileFile &profile_file);
}
