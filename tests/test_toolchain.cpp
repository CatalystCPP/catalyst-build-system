#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "catalyst/utils/toolchain.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

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
