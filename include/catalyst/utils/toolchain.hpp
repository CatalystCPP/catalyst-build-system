#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace catalyst::toolchain {

struct ToolchainDef {
    std::string name = "gcc_clang";

    struct Extensions {
        std::string object = ".o";
        std::string executable = "";
        std::string static_lib = ".a";
        std::string shared_lib = ".so";
        std::string static_lib_prefix = "lib";
        std::string shared_lib_prefix = "lib";
    } extensions;

    struct Flags {
        std::string include_dir = "-I{path}";
        std::string lib_dir = "-L{path}";
        std::string lib = "-l{name}";
        std::string define = "-D{name}={value}";
        std::string define_empty = "-D{name}";
    } flags;

    struct CompilerDef {
        std::string executable;
        std::string command;
    };

    struct Compiler {
        CompilerDef c = {.executable = "clang",
                         .command = "{cc} {cflags} -MMD -MF {object}.d -c {source} -o {object} {includes} {defines}"};
        CompilerDef cxx = {.executable = "clang++",
                           .command =
                               "{cxx} {cxxflags} -MMD -MF {object}.d -c {source} -o {object} {includes} {defines}"};
    } compiler;

    struct Linker {
        std::string executable = "clang++";
        std::string executable_command = "{linker} {objects} -o {output} {ldflags} {lib_dirs} {libs}";
        std::string shared_lib_command = "{linker} -shared {objects} -o {output} {ldflags} {lib_dirs} {libs}";
    } linker;

    struct Archiver {
        std::string executable = "ar";
        std::string command = "{archiver} rcs {output} {objects}";
    } archiver;
};

// Expands a template string replacing {key} with the corresponding value from vars
std::string expand_template(std::string_view tmpl, const std::unordered_map<std::string_view, std::string> &vars);

// Parse a ToolchainDef from a YAML file
std::expected<ToolchainDef, std::string> parse_toolchain(const std::filesystem::path &path);

} // namespace catalyst::toolchain
