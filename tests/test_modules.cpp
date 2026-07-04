#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "catalyst/subcommands/generate.hpp"

using namespace catalyst::generate;

namespace {

constexpr std::string_view P1689_INTERFACE_WITH_IMPORT = R"json({
  "revision": 0,
  "rules": [
    {
      "primary-output": "obj/bar.o",
      "provides": [{ "logical-name": "bar", "is-interface": true, "source-path": "src/bar.cppm" }],
      "requires": [{ "logical-name": "foo" }]
    }
  ],
  "version": 1
})json";

constexpr std::string_view P1689_IMPORTER_ONLY = R"json({
  "rules": [
    {
      "primary-output": "obj/main.o",
      "requires": [{ "logical-name": "foo" }, { "logical-name": "bar" }, { "logical-name": "foo" }]
    }
  ]
})json";

constexpr std::string_view P1689_PLAIN_TU = R"json({
  "rules": [{ "primary-output": "obj/plain.o" }],
  "version": 1
})json";

} // namespace

TEST_CASE("P1689 parsing", "[modules]") {
    SECTION("Interface unit that also imports") {
        auto info = modules::parseP1689(P1689_INTERFACE_WITH_IMPORT);
        REQUIRE(info.has_value());
        REQUIRE(info->provides == "bar");
        REQUIRE(info->required == std::vector<std::string>{"foo"});
    }

    SECTION("Importer only, duplicate requires are deduped") {
        auto info = modules::parseP1689(P1689_IMPORTER_ONLY);
        REQUIRE(info.has_value());
        REQUIRE(info->provides.empty());
        REQUIRE(info->required == std::vector<std::string>{"foo", "bar"});
    }

    SECTION("TU that touches no modules") {
        auto info = modules::parseP1689(P1689_PLAIN_TU);
        REQUIRE(info.has_value());
        REQUIRE(info->provides.empty());
        REQUIRE(info->required.empty());
    }

    SECTION("Malformed input is an error") {
        auto info = modules::parseP1689("{ not json");
        REQUIRE_FALSE(info.has_value());
    }
}

TEST_CASE("Module helpers", "[modules]") {
    SECTION("BMI path") {
        REQUIRE(modules::bmiPath("foo") == "obj/foo.pcm");
        REQUIRE(modules::bmiPath("foo.util") == "obj/foo.util.pcm");
        // partition separator is sanitized
        REQUIRE(modules::bmiPath("foo:part") == "obj/foo-part.pcm");
    }

    SECTION("Interface extensions") {
        REQUIRE(modules::isInterfaceExtension("a.cppm"));
        REQUIRE(modules::isInterfaceExtension("a.ixx"));
        REQUIRE(modules::isInterfaceExtension("a.mpp"));
        REQUIRE(modules::isInterfaceExtension("a.cxxm"));
        REQUIRE_FALSE(modules::isInterfaceExtension("a.cpp"));
        REQUIRE_FALSE(modules::isInterfaceExtension("a.h"));
    }

    SECTION("Clang-native interface extensions need no -x c++-module") {
        REQUIRE_FALSE(modules::needsExplicitModuleType("a.cppm"));
        REQUIRE_FALSE(modules::needsExplicitModuleType("a.cxxm"));
        REQUIRE(modules::needsExplicitModuleType("a.ixx"));
        REQUIRE(modules::needsExplicitModuleType("a.mpp"));
        REQUIRE(modules::needsExplicitModuleType("a.cpp"));
    }
}

TEST_CASE("COB writer module emission", "[modules][writers]") {
    std::ostringstream out;
    buildwriters::DerivedWriter<buildwriters::TargetType::COB> writer(out);

    SECTION("No extra args or implicit deps: format unchanged") {
        REQUIRE(writer.addBuild({"obj/main.o"}, "cxx_compile", {"main.cpp"}).has_value());
        REQUIRE(out.str() == "cxx|main.cpp|obj/main.o\n");
    }

    SECTION("Implicit deps become opaque '!' inputs, extra args the 4th field") {
        REQUIRE(writer
                    .addBuild({"obj/main.o"},
                              "cxx_compile",
                              {"main.cpp"},
                              {"obj/foo.o", "obj/bar.o"},
                              {"-fmodule-file=foo=obj/foo.pcm", "-fmodule-file=bar=obj/bar.pcm"})
                    .has_value());
        REQUIRE(out.str()
                == "cxx|main.cpp,!obj/foo.o,!obj/bar.o|obj/main.o|"
                   "-fmodule-file=foo=obj/foo.pcm -fmodule-file=bar=obj/bar.pcm\n");
    }

    SECTION("Interface unit: -fmodule-output only, no opaque inputs") {
        REQUIRE(writer.addBuild({"obj/foo.o"}, "cxx_compile", {"foo.cppm"}, {}, {"-fmodule-output=obj/foo.pcm"})
                    .has_value());
        REQUIRE(out.str() == "cxx|foo.cppm|obj/foo.o|-fmodule-output=obj/foo.pcm\n");
    }
}

TEST_CASE("Ninja writer module emission", "[modules][writers]") {
    std::ostringstream out;
    buildwriters::DerivedWriter<buildwriters::TargetType::Ninja> writer(out);

    SECTION("Extra args become a per-edge variable") {
        REQUIRE(
            writer
                .addBuild({"obj/main.o"}, "cxx_compile", {"main.cpp"}, {"obj/foo.o"}, {"-fmodule-file=foo=obj/foo.pcm"})
                .has_value());
        REQUIRE(out.str()
                == "build obj/main.o: cxx_compile main.cpp | obj/foo.o\n"
                   "  extra_args = -fmodule-file=foo=obj/foo.pcm\n");
    }

    SECTION("No extra args: no edge variable emitted") {
        REQUIRE(writer.addBuild({"obj/main.o"}, "cxx_compile", {"main.cpp"}).has_value());
        REQUIRE(out.str() == "build obj/main.o: cxx_compile main.cpp\n");
    }
}

TEST_CASE("Make writer module emission", "[modules][writers]") {
    std::ostringstream out;
    buildwriters::DerivedWriter<buildwriters::TargetType::Make> writer(out);

    SECTION("Extra args become a private target-specific variable") {
        REQUIRE(
            writer
                .addBuild({"obj/main.o"}, "cxx_compile", {"main.cpp"}, {"obj/foo.o"}, {"-fmodule-file=foo=obj/foo.pcm"})
                .has_value());
        REQUIRE(out.str()
                == "obj/main.o : private extra_args := -fmodule-file=foo=obj/foo.pcm\n"
                   "obj/main.o : main.cpp | obj/foo.o\n"
                   "\t$(cxx_compile)\n\n");
    }

    SECTION("No extra args: no variable line emitted") {
        REQUIRE(writer.addBuild({"obj/main.o"}, "cxx_compile", {"main.cpp"}).has_value());
        REQUIRE(out.str()
                == "obj/main.o : main.cpp\n"
                   "\t$(cxx_compile)\n\n");
    }
}
