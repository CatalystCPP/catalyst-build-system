#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"

namespace catalyst::init {
struct Parse {
    enum class Type : std::uint8_t { BINARY, STATICLIB, SHAREDLIB, INTERFACE };
    enum class IdeType : std::uint8_t { vsc, clion };

    std::string name{std::filesystem::current_path().filename().string()};
    std::filesystem::path path{std::filesystem::current_path()};
    Type type{Parse::Type::BINARY};
    std::string version{"0.0.1"};
    std::string description{"Your description goes here."};
    std::string provides;
    struct {
        std::vector<std::string> include{{"include"}};
        std::vector<std::string> source{{"src"}};
        std::string build{"build"};
    } dirs;
    std::string profile{"common"}; // only allow initializing one profile at a time.
    std::vector<Parse::IdeType> ides;
    bool force_emit_ide{false};
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);

Result<void> invokeIDEConfigEmitters(const Parse &parse_args);
template <Parse::IdeType Ide_T> Result<void> emitIDEConfig(const Parse &) {
    static_assert(Ide_T == Parse::IdeType::vsc || Ide_T == Parse::IdeType::clion,
                  "emitIDEConfig is not implemented for this IdeType");
}

template <> Result<void> emitIDEConfig<Parse::IdeType::vsc>(const Parse &parse_args);
template <> Result<void> emitIDEConfig<Parse::IdeType::clion>(const Parse &parse_args);
} // namespace catalyst::init
