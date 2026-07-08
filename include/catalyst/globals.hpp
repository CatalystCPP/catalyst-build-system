#pragma once
#include <string>
#define STRINGIFY_IMPL(x) #x
#define TOSTRING(x) STRINGIFY_IMPL(x)

namespace catalyst {
#ifdef CATALYST_BUILD_SYS
/// @brief Current Project Version
constexpr std::string CATALYST_VERSION = TOSTRING(CATALYST_PROJ_VER);
#else
/// @brief Current Project Version
constexpr std::string CATALYST_VERSION = "1.9.0";
#endif
}; // namespace catalyst
