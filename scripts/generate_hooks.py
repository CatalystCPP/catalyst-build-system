import argparse
import sys

hook_types = [
    "CommandHook",
    "ScriptHook",
    "CatalystHook",
    "CodegenHook"
]

functions = [
    "preBuild",
    "postBuild",
    "onBuildFailure",
    "preGenerate",
    "postGenerate",
    "preFetch",
    "postFetch",
    "preClean",
    "postClean",
    "preRun",
    "postRun",
    "preTest",
    "postTest",
    "preBench",
    "postBench",
    "prePack",
    "postPack"
]


def generate_content():
    hook_type_headers = [
        f"Result<void> execute{
            hook_type}(ryml::ConstNodeRef item, std::string_view hook_name);"
        for hook_type in hook_types
    ]

    hook_function_headers = [
        f"Result<void> {
            function}(const utils::yaml::Configuration &profile_comp);"
        for function in functions
    ]
    content = f"""#pragma once
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <ryml/ryml.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::hooks {{

{"\n".join(hook_type_headers)}
std::vector<std::string> shellCmd(std::string_view cmd);

{"\n".join(hook_function_headers)}
}}; // namespace catalyst::hooks
"""
    return content


def main():
    parser = argparse.ArgumentParser(
        description="Generate include/catalyst/dispatch.hpp")
    parser.add_argument(
        "-o", "--output",
        help="Output file path. If not specified, output is written to stdout."
    )
    args = parser.parse_args()

    content = generate_content()

    if args.output:
        try:
            with open(args.output, "w", encoding="utf-8") as f:
                f.write(content)
            print(f"Successfully generated dispatch header at: {args.output}")
        except Exception as e:
            print(f"Error writing to file {args.output}: {e}", file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
