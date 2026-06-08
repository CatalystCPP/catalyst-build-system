#pragma once
#include <cstdint>
#include <string>

namespace catalyst::utils::os {
// NOLINTBEGIN(cppcoreguidelines-use-default-member-init,modernize-use-default-member-init,cppcoreguidelines-prefer-member-initializer)
enum class OperatingSystem : std::uint8_t { Linux, MacOS, Windows, Unknown };
enum class Architecture : std::uint8_t { x86, x86_64, ARM32, ARM64, Unknown };
struct OSInfo {
    OperatingSystem os;
    Architecture arch;
    constexpr OSInfo() : os(OperatingSystem::Unknown), arch(Architecture::Unknown) {
#ifdef _WIN32
        os = OperatingSystem::Windows;
#elif defined(__APPLE__) || defined(__MACH__)
        os = OperatingSystem::MacOS;
#elif defined(__linux__)
        os = OperatingSystem::Linux;
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) ||           \
    defined(_M_AMD64)
        arch = Architecture::x86_64;
#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(__i486__) || defined(__i586__) ||               \
    defined(__i686__) || defined(_M_I86) || defined(_M_IX86) || defined(__X86__) || defined(_X86_) ||                  \
    defined(__THW_INTEL__) || defined(__I86__) || defined(__INTEL__)
        arch = Architecture::x86;
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm64__)
        arch = Architecture::ARM64;
#elif defined(__arm__) || defined(_M_ARM)
        arch = Architecture::ARM32;
#endif
    }
    // NOLINTEND(cppcoreguidelines-use-default-member-init,modernize-use-default-member-init,cppcoreguidelines-prefer-member-initializer)
};

bool isCommandInstalled(const std::string &command);

}; // namespace catalyst::utils::os

