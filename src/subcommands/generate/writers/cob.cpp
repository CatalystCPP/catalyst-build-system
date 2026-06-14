#include <algorithm>
#include <expected>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "catalyst/utils/result.hpp"
#include "catalyst/subcommands/generate.hpp"

namespace {
std::string escape(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        if (c == '\n') {
            result.append("\\n");
        } else {
            result.push_back(c);
        }
    }
    return result;
}
} // namespace

namespace catalyst::generate::buildwriters {

template <>
Result<void> DerivedWriter<TargetType::COB>::addVariable(std::string_view name,
                                                                             std::string_view value) {
    std::println(stream, "DEF|{}|{}", name, escape(value));
    return {};
}

template <>
Result<void> DerivedWriter<TargetType::COB>::addRule([[maybe_unused]] std::string_view name,
                                                                         [[maybe_unused]] std::string_view command,
                                                                         [[maybe_unused]] std::string_view description,
                                                                         [[maybe_unused]] std::string_view depfile,
                                                                         [[maybe_unused]] std::string_view deps) {
    // COB has built-in rules mapped to specific keys (cc, cxx, etc.).
    // We do not need to define them in the manifest, but we rely on the
    // generator to pass the correct standard rule names in add_build.
    return {};
}

template <>
Result<void>
DerivedWriter<TargetType::COB>::addBuild(const std::vector<std::string> &outputs,
                                         std::string_view rule,
                                         const std::vector<std::string> &inputs,
                                         [[maybe_unused]] const std::vector<std::string> &implicit_deps) {
    if (outputs.empty()) {
        return std::unexpected("COB writer requires at least one output.");
    }
    // COB supports only one output per step primarily.
    // If there are multiple, we might strictly only support the first one, or it's a usage error.
    // For now, we take the first one.
    std::string_view output_file = outputs[0];

    std::string step_type;
    if (rule == "cxx_compile") {
        step_type = "cxx";
    } else if (rule == "cc_compile") {
        step_type = "cc";
    } else if (rule == "binary_link") {
        step_type = "ld";
    } else if (rule == "static_link") {
        step_type = "ar";
    } else if (rule == "shared_link") {
        step_type = "sld";
    } else {
        // Fallback or error?
        // If the rule is unknown, COB might not support it.
        // Let's default to erroring if it's not one of the known ones,
        // OR return unexpected.
        return std::unexpected(std::string("Unknown rule type for COB: ") + std::string(rule));
    }

    // Join inputs with comma
    std::string input_list;
    for (size_t i = 0; i < inputs.size(); ++i) {
        input_list.append(inputs[i]);
        if (i < inputs.size() - 1) {
            input_list.push_back(',');
        }
    }

    std::println(stream, "{}|{}|{}", step_type, input_list, output_file);
    return {};
}

template <> void DerivedWriter<TargetType::COB>::addComment(std::string_view comment) {
    std::println(stream, "# {}", escape(comment));
}

template <> void DerivedWriter<TargetType::COB>::addDefault([[maybe_unused]] std::string_view target) {
    // COB does not have a 'default' target directive.
    // It builds the graph required to produce the requested targets or all if unspecified?
    // Typical low-level builders might just build everything defined
    // or rely on CLI arguments.
}

template class DerivedWriter<TargetType::COB>;
} // namespace catalyst::generate::buildwriters
