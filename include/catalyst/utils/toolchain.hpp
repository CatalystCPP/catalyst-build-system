#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "catalyst/utils/result.hpp"

namespace catalyst::toolchain {

struct ToolchainDef {
    std::string name = "gcc";

    struct Extensions {
        std::string object = ".o";
        std::string executable = "";
        std::string static_lib = ".a";
        std::string shared_lib = ".so";
        std::string static_lib_prefix = "lib";
        std::string shared_lib_prefix = "lib";
        std::vector<std::string> cpp_sources = {".cpp", ".cxx", ".cc", ".cupp"};
        std::vector<std::string> headers = {".h", ".hpp", ".hxx", ".hh", ".ipp", ".inl", ".tpp", ".cuh", ".tcc"};
        std::vector<std::string> c_sources = {".c", ".cu"};
        std::vector<std::string> module_interfaces = {".cppm", ".ixx", ".mpp", ".cxxm"};
        std::vector<std::string> clang_modules = {".cppm", ".cxxm"};
        std::string bmi = ".pcm";
        std::vector<std::string> library_scan;
        std::vector<std::string> shell_scripts = {".bat", ".cmd"};
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
        std::string flags; // base compiler flags, interpolated as {cflags}/{cxxflags}
        std::string command;
    };

    struct Compiler {
        CompilerDef c = {.executable = "cc",
                         .flags = {},
                         .command = "{cc} {cflags} -MMD -MF {object}.d -c {source} -o {object} {includes} {defines}"};
        CompilerDef cxx = {.executable = "c++",
                           .flags = {},
                           .command =
                               "{cxx} {cxxflags} -MMD -MF {object}.d -c {source} -o {object} {includes} {defines}"};
    } compiler;

    struct Linker {
        std::string executable = "c++";
        std::string flags; // base linker flags, interpolated as {ldflags}
        std::string executable_command = "{linker} {objects} -o {output} {ldflags} {lib_dirs} {libs}";
        std::string shared_lib_command = "{linker} -shared {objects} -o {output} {ldflags} {lib_dirs} {libs}";
    } linker;

    struct Archiver {
        std::string executable = "ar";
        std::string command = "{archiver} rcs {output} {objects}";
    } archiver;
};

// Expands a template string replacing {key} with the corresponding value from vars
std::string expandTemplate(std::string_view tmpl, const std::unordered_map<std::string_view, std::string> &vars);

// Parse a ToolchainDef from a YAML file
Result<ToolchainDef> parseToolchain(const std::filesystem::path &path);

} // namespace catalyst::toolchain
