#include <catch2/catch_test_macros.hpp>

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
