#include <expected>
#include <print>
#include <string>
#include <string_view>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/result.hpp"

namespace {
std::string escape(std::string_view str) {
    constexpr double ESCAPE_TUNING_FACTOR = 1.25;
    std::string result;
    result.reserve(static_cast<size_t>(str.size() * ESCAPE_TUNING_FACTOR)); // NOTE: allow some extra space for escape
                                                                            // sequences
                                                                            // PERF: subject to tuning
    for (char c : str) {
        switch (c) {
            case '$':
                result.append("$$");
                break;
            case ' ':
                result.append("$ ");
                break;
            case ':':
                result.append("$:");
                break;
            case '\n':
                result.append("$\n");
                break;
            default:
                result.push_back(c);
                break;
        }
    }
    return result;
}
} // namespace

namespace catalyst::generate::buildwriters {
template <> Result<void> DerivedWriter<TargetType::Ninja>::addVariable(std::string_view name, std::string_view value) {
    static auto escape_quotes = [](std::string_view str) -> std::string {
        std::string result;
        constexpr double ESCAPE_TUNING_FACTOR = 1.25;
        result.reserve(
            static_cast<size_t>(str.size() * ESCAPE_TUNING_FACTOR)); // NOTE: allow some extra space for escape
        for (char c : str) {
            switch (c) {
                case '"':
                    result.append("\\\"");
                    break;
                default:
                    result.push_back(c);
                    break;
            }
        }
        return result;
    };
    std::println(stream, "{} = {}", name, escape_quotes(value));
    return {};
}

template <>
Result<void> DerivedWriter<TargetType::Ninja>::addRule(std::string_view name,
                                                       std::string_view command,
                                                       std::string_view description,
                                                       std::string_view depfile,
                                                       std::string_view deps) {
    std::println(stream, "rule {}\n  command = {} ", name, command);
    if (!description.empty()) {
        std::println(stream, "  description = {}", description);
    }
    if (!depfile.empty()) {
        std::println(stream, "  depfile = {}", depfile);
    }
    if (!deps.empty()) {
        std::println(stream, "  deps = {}", deps);
    }
    std::println(stream);
    return {};
}

template <>
Result<void> DerivedWriter<TargetType::Ninja>::addBuild(const std::vector<std::string> &outputs,
                                                        std::string_view rule,
                                                        const std::vector<std::string> &inputs,
                                                        const std::vector<std::string> &implicit_deps) {
    std::print(stream, "build");
    for (const auto &out : outputs)
        std::print(stream, " {}", escape(out));

    std::print(stream, ": {}", rule);

    for (const auto &in : inputs)
        std::print(stream, " {}", escape(in));

    if (!implicit_deps.empty()) {
        std::print(stream, " |");
        for (const auto &dep : implicit_deps)
            std::print(stream, " {}", escape(dep));
    }

    std::println(stream);
    return {};
}

template <> void DerivedWriter<TargetType::Ninja>::addComment(std::string_view comment) {
    std::println(stream, "# {}", comment);
}

template <> void DerivedWriter<TargetType::Ninja>::addDefault(std::string_view target) {
    std::println(stream, "default {}", escape(target));
}

template class DerivedWriter<TargetType::Ninja>;
} // namespace catalyst::generate::buildwriters
