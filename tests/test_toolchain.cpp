#include <filesystem>
#include <fstream>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "catalyst/utils/toolchain.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::toolchain {
inline std::string expand_template(std::string_view tmpl,
                                   const std::unordered_map<std::string_view, std::string> &vars) {
    return expandTemplate(tmpl, vars);
}
inline Result<ToolchainDef> parse_toolchain(const std::filesystem::path &path) {
    return parseToolchain(path);
}
} // namespace catalyst::toolchain

using namespace catalyst::toolchain;

TEST_CASE("Template Expansion", "[toolchain]") {
    SECTION("Single replacement") {
        std::string res = expand_template("-I{path}", {{"path", "/usr/include"}});
        REQUIRE(res == "-I/usr/include");
    }

    SECTION("Multiple replacements") {
        std::string res = expand_template("{cxx} -c {source} -o {object}",
                                          {{"cxx", "clang++"}, {"source", "main.cpp"}, {"object", "main.o"}});
        REQUIRE(res == "clang++ -c main.cpp -o main.o");
    }

    SECTION("Missing placeholder replaces with empty") {
        std::string res = expand_template("{cxx} {missing} -o {object}", {{"cxx", "clang++"}, {"object", "main.o"}});
        REQUIRE(res == "clang++  -o main.o");
    }

    SECTION("No placeholders") {
        std::string res = expand_template("just a string", {{"unused", "value"}});
        REQUIRE(res == "just a string");
    }

    SECTION("Incomplete brace is ignored") {
        std::string res = expand_template("this { is not a placeholder", {{"path", "value"}});
        REQUIRE(res == "this { is not a placeholder");
    }
}

TEST_CASE("YAML Parsing", "[toolchain]") {
    std::filesystem::path temp_yaml = std::filesystem::temp_directory_path() / "test_toolchain.yaml";
    std::ofstream out(temp_yaml);
    out << R"(
toolchain:
  name: "msvc"
  extensions:
    object: ".obj"
  flags:
    include_dir: "/I\"{path}\""
    rpath: "/RPATH:\"{path}\""
  compiler:
    cxx:
      executable: "cl.exe"
      command: "cl.exe /c {source}"
)";
    out.close();

    auto tc_res = parse_toolchain(temp_yaml);
    REQUIRE(tc_res.has_value());
    auto tc = tc_res.value();

    REQUIRE(tc.name == "msvc");
    REQUIRE(tc.extensions.object == ".obj");
    REQUIRE(tc.flags.include_dir == "/I\"{path}\"");
    REQUIRE(tc.flags.rpath == "/RPATH:\"{path}\"");
    REQUIRE(tc.compiler.cxx.executable == "cl.exe");
    REQUIRE(tc.compiler.cxx.command == "cl.exe /c {source}");
    // Check that defaults were kept for unspecified fields
    REQUIRE(tc.extensions.static_lib == ".a");
    // Flags default to empty when unspecified
    REQUIRE(tc.compiler.c.flags.empty());
    REQUIRE(tc.compiler.cxx.flags.empty());
    REQUIRE(tc.linker.flags.empty());

    std::filesystem::remove(temp_yaml);
}

TEST_CASE("YAML Parsing reads compiler and linker flags", "[toolchain]") {
    std::filesystem::path temp_yaml = std::filesystem::temp_directory_path() / "test_toolchain_flags.yaml";
    std::ofstream out(temp_yaml);
    out << R"(
toolchain:
  name: "with-flags"
  compiler:
    c:
      flags: "-std=c17 -Wall"
    cxx:
      flags: "-std=c++23 -Wall -Wextra"
  linker:
    flags: "-static -fuse-ld=mold"
)";
    out.close();

    auto tc_res = parse_toolchain(temp_yaml);
    REQUIRE(tc_res.has_value());
    auto tc = tc_res.value();

    REQUIRE(tc.compiler.c.flags == "-std=c17 -Wall");
    REQUIRE(tc.compiler.cxx.flags == "-std=c++23 -Wall -Wextra");
    REQUIRE(tc.linker.flags == "-static -fuse-ld=mold");

    std::filesystem::remove(temp_yaml);
}

TEST_CASE("YAML Parsing reads new extension properties", "[toolchain]") {
    std::filesystem::path temp_yaml = std::filesystem::temp_directory_path() / "test_toolchain_extensions.yaml";
    std::ofstream out(temp_yaml);
    out << R"(
toolchain:
  name: "custom-extensions"
  extensions:
    cpp_sources:
      - ".cpp"
      - ".cxx"
      - ".cc"
    headers:
      - ".h"
      - ".hpp"
    c_sources:
      - ".c"
    module_interfaces:
      - ".cppm"
      - ".ixx"
    clang_modules:
      - ".cppm"
    bmi: ".pcm"
    library_scan:
      - ".a"
      - ".so"
    shell_scripts:
      - ".bat"
)";
    out.close();

    auto tc_res = parse_toolchain(temp_yaml);
    REQUIRE(tc_res.has_value());
    auto tc = tc_res.value();

    REQUIRE(tc.extensions.cpp_sources == std::vector<std::string>{".cpp", ".cxx", ".cc"});
    REQUIRE(tc.extensions.headers == std::vector<std::string>{".h", ".hpp"});
    REQUIRE(tc.extensions.c_sources == std::vector<std::string>{".c"});
    REQUIRE(tc.extensions.module_interfaces == std::vector<std::string>{".cppm", ".ixx"});
    REQUIRE(tc.extensions.clang_modules == std::vector<std::string>{".cppm"});
    REQUIRE(tc.extensions.bmi == ".pcm");
    REQUIRE(tc.extensions.library_scan == std::vector<std::string>{".a", ".so"});
    REQUIRE(tc.extensions.shell_scripts == std::vector<std::string>{".bat"});

    std::filesystem::remove(temp_yaml);
}

TEST_CASE("Resolved toolchain serialization is deterministic and complete", "[toolchain]") {
    namespace fs = std::filesystem;

    ToolchainDef tc;
    tc.name = "custom \"snapshot\"\\path\nnext-line";
    tc.extensions.object = ".object";
    tc.extensions.executable = ".executable";
    tc.extensions.static_lib = ".static";
    tc.extensions.shared_lib = ".shared";
    tc.extensions.static_lib_prefix = "static-";
    tc.extensions.shared_lib_prefix = "shared-";
    tc.extensions.cpp_sources = {".cpp-custom"};
    tc.extensions.headers = {".header-custom"};
    tc.extensions.c_sources = {".c-custom"};
    tc.extensions.module_interfaces = {".module-custom"};
    tc.extensions.clang_modules = {".clang-module-custom"};
    tc.extensions.bmi = ".bmi-custom";
    tc.extensions.library_scan = {".library-custom"};
    tc.extensions.shell_scripts = {".shell-custom", ".shell\\windows"};
    tc.flags.include_dir = "include {path}";
    tc.flags.lib_dir = "lib-dir {path}";
    tc.flags.lib = "lib {name}";
    tc.flags.rpath = "rpath {path}";
    tc.flags.define = "define {name} {value}";
    tc.flags.define_empty = "define {name}";
    tc.compiler.c = {.executable = "custom-cc", .flags = "custom-c-flags", .command = "custom\tc command"};
    tc.compiler.cxx = {.executable = "custom-cxx", .flags = "custom-cxx-flags", .command = "custom cxx command"};
    tc.linker.executable = "custom-linker";
    tc.linker.flags = "custom-linker-flags";
    tc.linker.executable_command = "custom executable link command";
    tc.linker.shared_lib_command = "custom shared link command";
    tc.archiver.executable = "custom-archiver";
    tc.archiver.command = "custom archive command";

    const std::string serialized = serializeToolchain(tc);
    REQUIRE(serializeToolchain(tc) == serialized);
    REQUIRE(serialized.find("extends") == std::string::npos);
    REQUIRE(serializeToolchainStore(tc, "cob") != serializeToolchainStore(tc, "ninja"));

    const fs::path temp_dir = fs::temp_directory_path() / "catalyst_resolved_toolchain_serialization";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);
    const fs::path snapshot_path = temp_dir / RESOLVED_TOOLCHAIN_STORE_FILENAME;
    {
        std::ofstream snapshot{snapshot_path};
        snapshot << serializeToolchainStore(tc, "cob");
    }

    auto reparsed = parseToolchain(snapshot_path);
    REQUIRE(reparsed.has_value());
    REQUIRE(*reparsed == tc);

    auto defaults = resolveToolchain(std::nullopt);
    REQUIRE(defaults.has_value());
    REQUIRE_FALSE(defaults->extensions.library_scan.empty());
    {
        std::ofstream snapshot{snapshot_path};
        snapshot << serializeToolchain(*defaults);
    }
    auto reparsed_defaults = parseToolchain(snapshot_path);
    REQUIRE(reparsed_defaults.has_value());
    REQUIRE(*reparsed_defaults == *defaults);
    fs::remove_all(temp_dir);
}

TEST_CASE("Resolved toolchain serialization compares effective inheritance", "[toolchain]") {
    namespace fs = std::filesystem;

    const fs::path temp_dir = fs::temp_directory_path() / "catalyst_resolved_toolchain_inheritance";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    const fs::path base_path = temp_dir / "base.yaml";
    const fs::path child_path = temp_dir / "child.yaml";
    {
        std::ofstream base{base_path};
        base << R"(toolchain:
  compiler:
    cxx:
      executable: clang++
      flags: -O2
)";
        std::ofstream child{child_path};
        child << R"(toolchain:
  extends: base.yaml
  compiler:
    cxx:
      flags: -O0
)";
    }

    auto original = parseToolchain(child_path);
    REQUIRE(original.has_value());
    const std::string original_snapshot = serializeToolchain(*original);

    // The changed base flag is overridden by the leaf, so the effective toolchain is unchanged.
    {
        std::ofstream base{base_path};
        base << R"(toolchain:
  compiler:
    cxx:
      executable: "clang++"
      flags: "-O3"
)";
    }
    auto equivalent = parseToolchain(child_path);
    REQUIRE(equivalent.has_value());
    REQUIRE(serializeToolchain(*equivalent) == original_snapshot);

    // The compiler executable is inherited, so changing it changes the effective snapshot.
    {
        std::ofstream base{base_path};
        base << R"(toolchain:
  compiler:
    cxx:
      executable: g++
      flags: -O3
)";
    }
    auto changed = parseToolchain(child_path);
    REQUIRE(changed.has_value());
    REQUIRE(serializeToolchain(*changed) != original_snapshot);
    fs::remove_all(temp_dir);
}

TEST_CASE("Configuration preserves manifest.toolchain", "[toolchain][configuration]") {
    namespace fs = std::filesystem;

    fs::path temp_root = fs::temp_directory_path() / "catalyst_toolchain_config_test";
    fs::create_directories(temp_root);

    std::ofstream out(temp_root / "CATALYST.yaml");
    out << R"(
common:
  manifest:
    name: "demo"
    toolchain: "msvc.yaml"
    dirs:
      source: ["src"]
      include: []
      build: "build"
)";
    out.close();

    catalyst::utils::yaml::Configuration config({"common"}, temp_root);
    REQUIRE(config.getString("manifest.toolchain").has_value());
    REQUIRE(config.getString("manifest.toolchain").value() == "msvc.yaml");

    fs::remove_all(temp_root);
}

TEST_CASE("Toolchain inheritance (extends)", "[toolchain]") {
    namespace fs = std::filesystem;
    fs::path temp_dir = fs::temp_directory_path() / "catalyst_test_extends";
    fs::create_directories(temp_dir);

    SECTION("Leaf overrides subset of base keys, inherited keys survive") {
        std::ofstream out_base(temp_dir / "base.yaml");
        out_base << R"(
toolchain:
  name: "base-tc"
  extensions:
    object: ".obj"
    executable: ".exe"
  compiler:
    cxx:
      executable: "clang++"
      command: "clang++ {source}"
)";
        out_base.close();

        std::ofstream out_child(temp_dir / "child.yaml");
        out_child << R"(
toolchain:
  extends: "base.yaml"
  name: "child-tc"
  compiler:
    cxx:
      executable: "g++"
)";
        out_child.close();

        auto tc_res = parse_toolchain(temp_dir / "child.yaml");
        REQUIRE(tc_res.has_value());
        auto tc = tc_res.value();

        REQUIRE(tc.name == "child-tc");
        REQUIRE(tc.extensions.object == ".obj");                // Inherited
        REQUIRE(tc.extensions.executable == ".exe");            // Inherited
        REQUIRE(tc.compiler.cxx.executable == "g++");           // Overridden
        REQUIRE(tc.compiler.cxx.command == "clang++ {source}"); // Inherited
    }

    SECTION("Flag strings are replaced, not concatenated") {
        std::ofstream out_base(temp_dir / "base.yaml");
        out_base << R"(
toolchain:
  compiler:
    cxx:
      flags: "-std=c++20 -O2"
)";
        out_base.close();

        std::ofstream out_child(temp_dir / "child.yaml");
        out_child << R"(
toolchain:
  extends: "base.yaml"
  compiler:
    cxx:
      flags: "-O0"
)";
        out_child.close();

        auto tc_res = parse_toolchain(temp_dir / "child.yaml");
        REQUIRE(tc_res.has_value());
        auto tc = tc_res.value();

        REQUIRE(tc.compiler.cxx.flags == "-O0");
    }

    SECTION("Multi-level chain (a extends b extends c)") {
        std::ofstream out_c(temp_dir / "c.yaml");
        out_c << R"(
toolchain:
  name: "c-tc"
  extensions:
    object: ".o3"
    executable: ".exe3"
)";
        out_c.close();

        std::ofstream out_b(temp_dir / "b.yaml");
        out_b << R"(
toolchain:
  extends: "c.yaml"
  name: "b-tc"
  extensions:
    object: ".o2"
)";
        out_b.close();

        std::ofstream out_a(temp_dir / "a.yaml");
        out_a << R"(
toolchain:
  extends: "b.yaml"
  name: "a-tc"
)";
        out_a.close();

        auto tc_res = parse_toolchain(temp_dir / "a.yaml");
        REQUIRE(tc_res.has_value());
        auto tc = tc_res.value();

        REQUIRE(tc.name == "a-tc");
        REQUIRE(tc.extensions.object == ".o2");       // b overrides c
        REQUIRE(tc.extensions.executable == ".exe3"); // c survives
    }

    SECTION("extends path resolves relative to the child file, not the CWD") {
        fs::path sub_dir = temp_dir / "subdir";
        fs::create_directories(sub_dir);

        std::ofstream out_base(temp_dir / "base_rel.yaml");
        out_base << R"(
toolchain:
  name: "resolved-relative"
)";
        out_base.close();

        std::ofstream out_child(sub_dir / "child_rel.yaml");
        out_child << R"(
toolchain:
  extends: "../base_rel.yaml"
  name: "child-rel"
)";
        out_child.close();

        auto tc_res = parse_toolchain(sub_dir / "child_rel.yaml");
        REQUIRE(tc_res.has_value());
        auto tc = tc_res.value();

        REQUIRE(tc.name == "child-rel");
    }

    SECTION("Cycle (a -> b -> a) and self-extension are reported as errors") {
        std::ofstream out_a(temp_dir / "cycle_a.yaml");
        out_a << R"(
toolchain:
  extends: "cycle_b.yaml"
)";
        out_a.close();

        std::ofstream out_b(temp_dir / "cycle_b.yaml");
        out_b << R"(
toolchain:
  extends: "cycle_a.yaml"
)";
        out_b.close();

        auto tc_res = parse_toolchain(temp_dir / "cycle_a.yaml");
        REQUIRE(!tc_res.has_value());
        std::string err = tc_res.error();
        REQUIRE(err.find("Toolchain inheritance cycle detected") != std::string::npos);

        std::ofstream out_self(temp_dir / "self_ext.yaml");
        out_self << R"(
toolchain:
  extends: "self_ext.yaml"
)";
        out_self.close();

        auto tc_res_self = parse_toolchain(temp_dir / "self_ext.yaml");
        REQUIRE(!tc_res_self.has_value());
        std::string err_self = tc_res_self.error();
        REQUIRE(err_self.find("Toolchain inheritance cycle detected") != std::string::npos);
    }

    SECTION("Missing base file is reported with both paths") {
        std::ofstream out_child(temp_dir / "child_missing.yaml");
        out_child << R"(
toolchain:
  extends: "no_such_file.yaml"
)";
        out_child.close();

        auto tc_res = parse_toolchain(temp_dir / "child_missing.yaml");
        REQUIRE(!tc_res.has_value());
        std::string err = tc_res.error();
        REQUIRE(err.find("Failed to open base toolchain") != std::string::npos);
        REQUIRE(err.find("no_such_file.yaml") != std::string::npos);
        REQUIRE(err.find("child_missing.yaml") != std::string::npos);
    }

    SECTION("File with no extends behaves exactly as before") {
        std::ofstream out_no_ext(temp_dir / "no_ext.yaml");
        out_no_ext << R"(
toolchain:
  name: "standalone"
)";
        out_no_ext.close();

        auto tc_res = parse_toolchain(temp_dir / "no_ext.yaml");
        REQUIRE(tc_res.has_value());
        auto tc = tc_res.value();
        REQUIRE(tc.name == "standalone");
        // Verify default object extension
        REQUIRE(tc.extensions.object == ".o");
    }

    fs::remove_all(temp_dir);
}

TEST_CASE("Toolchain inheritance composition", "[toolchain]") {
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "test_inheritance_comp";
    std::filesystem::create_directories(temp_dir);

    // Write base toolchain
    std::ofstream out_base(temp_dir / "base.yaml");
    out_base << R"(
toolchain:
  name: "base"
  compiler:
    cxx:
      flags: "-std=c++23 -Wall -Wextra -O2"
)";
    out_base.close();

    SECTION("Wholesale overwrite via 'flags'") {
        std::ofstream out_child(temp_dir / "child.yaml");
        out_child << R"(
toolchain:
  extends: "base.yaml"
  name: "child"
  compiler:
    cxx:
      flags: "-O3"
)";
        out_child.close();

        auto res = parse_toolchain(temp_dir / "child.yaml");
        REQUIRE(res.has_value());
        REQUIRE(res->compiler.cxx.flags == "-O3");
    }

    SECTION("Append flags") {
        std::ofstream out_child(temp_dir / "child.yaml");
        out_child << R"(
toolchain:
  extends: "base.yaml"
  name: "child"
  compiler:
    cxx:
      flags_append: "-g -flto"
)";
        out_child.close();

        auto res = parse_toolchain(temp_dir / "child.yaml");
        REQUIRE(res.has_value());
        REQUIRE(res->compiler.cxx.flags == "-std=c++23 -Wall -Wextra -O2 -g -flto");
    }

    SECTION("Remove flags") {
        std::ofstream out_child(temp_dir / "child.yaml");
        out_child << R"(
toolchain:
  extends: "base.yaml"
  name: "child"
  compiler:
    cxx:
      flags_remove: "-O2 -Wall"
)";
        out_child.close();

        auto res = parse_toolchain(temp_dir / "child.yaml");
        REQUIRE(res.has_value());
        REQUIRE(res->compiler.cxx.flags == "-std=c++23 -Wextra");
    }

    SECTION("Append and Remove flags combined") {
        std::ofstream out_child(temp_dir / "child.yaml");
        out_child << R"(
toolchain:
  extends: "base.yaml"
  name: "child"
  compiler:
    cxx:
      flags_remove: "-O2"
      flags_append: "-O3 -g"
)";
        out_child.close();

        auto res = parse_toolchain(temp_dir / "child.yaml");
        REQUIRE(res.has_value());
        REQUIRE(res->compiler.cxx.flags == "-std=c++23 -Wall -Wextra -O3 -g");
    }

    SECTION("Flags as list") {
        std::ofstream out_child(temp_dir / "child.yaml");
        out_child << R"(
toolchain:
  extends: "base.yaml"
  name: "child"
  compiler:
    cxx:
      flags_remove: ["-O2", "-Wall"]
      flags_append: ["-O3", "-g"]
)";
        out_child.close();

        auto res = parse_toolchain(temp_dir / "child.yaml");
        REQUIRE(res.has_value());
        REQUIRE(res->compiler.cxx.flags == "-std=c++23 -Wextra -O3 -g");
    }

    SECTION("Wholesale overwrite flags as list") {
        std::ofstream out_child(temp_dir / "child.yaml");
        out_child << R"(
toolchain:
  extends: "base.yaml"
  name: "child"
  compiler:
    cxx:
      flags:
        - "-O3"
        - "-g"
)";
        out_child.close();

        auto res = parse_toolchain(temp_dir / "child.yaml");
        REQUIRE(res.has_value());
        REQUIRE(res->compiler.cxx.flags == "-O3 -g");
    }

    std::filesystem::remove_all(temp_dir);
}
