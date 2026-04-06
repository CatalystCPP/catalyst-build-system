#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <filesystem>

#include "catalyst/utils/toolchain.hpp"

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

    std::filesystem::remove(temp_yaml);
}
