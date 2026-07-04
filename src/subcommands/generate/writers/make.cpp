#include <expected>
#include <print>
#include <regex>
#include <string>
#include <string_view>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/result.hpp"

namespace {
std::string translateNinjaToMake(std::string_view command) {
    std::string result(command);
    // 1. Replace $in and $out with placeholders
    result = std::regex_replace(result, std::regex(R"(\$in\b)"), "___CATALYST_IN___");
    result = std::regex_replace(result, std::regex(R"(\$out\b)"), "___CATALYST_OUT___");

    // 2. Replace $var with $(var) for other variables
    result = std::regex_replace(result, std::regex(R"(\$([a-zA-Z0-9_]+))"), "$$($1)");

    // 3. Replace placeholders with their Make equivalents
    result = std::regex_replace(result, std::regex("___CATALYST_IN___"), "$$(filter-out %.h %.hpp, $$^)");
    result = std::regex_replace(result, std::regex("___CATALYST_OUT___"), "$$@");

    return result;
}

std::string escape(std::string_view str) {
    std::string result;
    for (char c : str) {
        if (c == ' ') {
            result.append("\\ ");
        } else {
            result.push_back(c);
        }
    }
    return result;
}
} // namespace

namespace catalyst::generate::buildwriters {
template <> Result<void> DerivedWriter<TargetType::Make>::addVariable(std::string_view name, std::string_view value) {
    std::println(stream, "{} := {}", name, value);
    return {};
}

template <>
Result<void> DerivedWriter<TargetType::Make>::addRule(std::string_view name,
                                                      std::string_view command,
                                                      [[maybe_unused]] std::string_view description,
                                                      [[maybe_unused]] std::string_view depfile,
                                                      [[maybe_unused]] std::string_view deps) {
    // In Make, we define a variable that holds the command.
    std::println(stream, "{} = {}", name, translateNinjaToMake(command));
    return {};
}

template <>
Result<void> DerivedWriter<TargetType::Make>::addBuild(const std::vector<std::string> &outputs,
                                                       std::string_view rule,
                                                       const std::vector<std::string> &inputs,
                                                       const std::vector<std::string> &implicit_deps,
                                                       const std::vector<std::string> &extra_args) {
    if (outputs.empty())
        return {};

    // Per-target flags, referenced as $(extra_args) in the rule command.
    // `private` keeps them from leaking into prerequisites' recipes.
    if (!extra_args.empty()) {
        for (const auto &out : outputs) {
            std::print(stream, "{} ", escape(out));
        }
        std::print(stream, ": private extra_args :=");
        for (const auto &arg : extra_args) {
            std::print(stream, " {}", arg);
        }
        std::println(stream);
    }

    for (const auto &out : outputs) {
        std::print(stream, "{} ", escape(out));
    }
    std::print(stream, ":");
    for (const auto &in : inputs) {
        std::print(stream, " {}", escape(in));
    }
    if (!implicit_deps.empty()) {
        std::print(stream, " |");
        for (const auto &dep : implicit_deps) {
            std::print(stream, " {}", escape(dep));
        }
    }
    std::println(stream);
    std::println(stream, "\t$({})", rule);
    std::println(stream);
    return {};
}

template <> void DerivedWriter<TargetType::Make>::addComment(std::string_view comment) {
    std::println(stream, "# {}", comment);
}

template <> void DerivedWriter<TargetType::Make>::addDefault(std::string_view target) {
    std::println(stream, ".DEFAULT_GOAL := {}", escape(target));
    std::println(stream, "-include $(wildcard obj/*.d)");
}

template class DerivedWriter<TargetType::Make>;
} // namespace catalyst::generate::buildwriters
