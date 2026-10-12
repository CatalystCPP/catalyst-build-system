#pragma once
#include <string>

namespace catalyst {
#ifdef CATALYST_BUILD_SYS
/// @brief Current Project Version
constexpr std::string CATALYST_VERSION = CATALYST_PROJ_VER;
#else
/// @brief Current Project Version
constexpr std::string CATALYST_VERSION = "2.1.0";
#endif
}; // namespace catalyst
