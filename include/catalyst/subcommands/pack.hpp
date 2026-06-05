#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include <CLI11.hpp>

namespace catalyst::pack {
enum class Generator : std::uint8_t {
    TGZ,
    ZIP,
    DEB,
    RPM,
    NSIS,
    WIX,
    DMG,
    STGZ,
    FREEBSD,
    APK,
    SEVEN_ZIP,
    TXZ,
    EXTERNAL
};

struct Parse {
    std::vector<std::string> profiles{"common"};
    std::filesystem::path source_path{std::filesystem::current_path()};
    std::filesystem::path target_path{"build/pack"};
    std::vector<Generator> generators;
    bool all_generators{false};
    bool silent{false};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &parse_args);
} // namespace catalyst::pack
