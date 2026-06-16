#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "catalyst/utils/toolchain.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

using namespace catalyst::toolchain;

TEST_CASE("Template Expansion", "[toolchain]") {
    SECTION("Single replacement") {
        std::string res = expand_template("-I{path}", {{"path", "/usr/include"}});
        REQUIRE(res == "-I/usr/include");
    }

    SECTION("Multiple replacements") {
        std::string res = expand_template("{cxx} -c {source} -o {object}", {
            {"cxx", "clang++"},
            {"source", "main.cpp"},
            {"object", "main.o"}
        });
        REQUIRE(res == "clang++ -c main.cpp -o main.o");
    }

    SECTION("Missing placeholder replaces with empty") {
        std::string res = expand_template("{cxx} {missing} -o {object}", {
            {"cxx", "clang++"},
            {"object", "main.o"}
        });
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
