// Unit tests for the rapidyaml wrapper layer (Phase 1 of the yaml-cpp port,
// see docs/yaml-cpp-to-rapidyaml-port.md). The getter semantics here are
// deliberately aligned with the yaml-cpp behavior pinned by
// test_yaml_characterization.cpp.

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace fs = std::filesystem;
using catalyst::utils::yaml::asBool;
using catalyst::utils::yaml::asInt;
using catalyst::utils::yaml::asString;
using catalyst::utils::yaml::asStringVector;
using catalyst::utils::yaml::child;
using catalyst::utils::yaml::emitYaml;
using catalyst::utils::yaml::loadFile;
using catalyst::utils::yaml::parseYaml;

namespace {

ryml::Tree parseOk(const char *yaml) {
    auto tree = parseYaml(yaml);
    REQUIRE(tree.has_value());
    return std::move(*tree);
}

} // namespace

TEST_CASE("ryml loadFile: reads and parses a file", "[yaml][ryml]") {
    fs::path path = fs::temp_directory_path() / "catalyst_ryml_loadfile.yaml";
    {
        std::ofstream out{path};
        out << "manifest:\n  name: loaded\n  version: 1.2.3\n";
    }

    auto tree = loadFile(path);
    REQUIRE(tree.has_value());
    CHECK(asString(tree->crootref()["manifest"]["name"]) == "loaded");
    CHECK(asString(tree->crootref()["manifest"]["version"]) == "1.2.3");

    // The tree owns its data: must stay valid after the source file vanishes.
    fs::remove(path);
    CHECK(asString(tree->crootref()["manifest"]["name"]) == "loaded");
}

TEST_CASE("ryml loadFile: missing file is an error, not an abort", "[yaml][ryml]") {
    auto tree = loadFile(fs::temp_directory_path() / "catalyst_ryml_no_such_file.yaml");
    REQUIRE_FALSE(tree.has_value());
    CHECK(tree.error().find("Failed to open YAML file") != std::string::npos);
}

TEST_CASE("ryml loadFile: parse failure is an error, not an abort", "[yaml][ryml]") {
    fs::path path = fs::temp_directory_path() / "catalyst_ryml_malformed.yaml";
    {
        std::ofstream out{path};
        out << "a: [unclosed\n";
    }

    auto tree = loadFile(path);
    fs::remove(path);
    REQUIRE_FALSE(tree.has_value());
    CHECK(tree.error().find("Failed to parse YAML file") != std::string::npos);
    // The filename must appear in the message so users can find the bad file.
    CHECK(tree.error().find("catalyst_ryml_malformed.yaml") != std::string::npos);
}

TEST_CASE("ryml asString: scalar, null, and container semantics", "[yaml][ryml]") {
    ryml::Tree tree = parseOk("str: hello\n"
                              "quoted_empty: \"\"\n"
                              "unquoted_null:\n"
                              "tilde: ~\n"
                              "explicit_null: null\n"
                              "map: {a: 1}\n"
                              "seq: [a, b]\n");
    ryml::ConstNodeRef root = tree.crootref();

    CHECK(asString(root["str"]) == "hello");
    // Matches yaml-cpp: quoted empty string is a string, unquoted empty is null
    CHECK(asString(root["quoted_empty"]) == "");
    CHECK(asString(root["unquoted_null"]) == std::nullopt);
    CHECK(asString(root["tilde"]) == std::nullopt);
    CHECK(asString(root["explicit_null"]) == std::nullopt);
    // Containers don't convert
    CHECK(asString(root["map"]) == std::nullopt);
    CHECK(asString(root["seq"]) == std::nullopt);
    // Missing keys probed via child() yield an invalid ref, not a crash
    CHECK(asString(child(root, "no_such_key")) == std::nullopt);
    CHECK(asString(ryml::ConstNodeRef{}) == std::nullopt);
}

TEST_CASE("ryml asInt: full-string integer conversion", "[yaml][ryml]") {
    ryml::Tree tree = parseOk("answer: 42\n"
                              "negative: -7\n"
                              "text: forty-two\n"
                              "partial: 12abc\n"
                              "version: 1.2.3\n"
                              "floaty: 1.5\n");
    ryml::ConstNodeRef root = tree.crootref();

    CHECK(asInt(root["answer"]) == 42);
    CHECK(asInt(root["negative"]) == -7);
    CHECK(asInt(root["text"]) == std::nullopt);
    CHECK(asInt(root["partial"]) == std::nullopt);
    CHECK(asInt(root["version"]) == std::nullopt);
    CHECK(asInt(root["floaty"]) == std::nullopt);
    CHECK(asInt(child(root, "no_such_key")) == std::nullopt);
}

TEST_CASE("ryml asBool: yaml-cpp flexible booleans", "[yaml][ryml]") {
    ryml::Tree tree = parseOk("t1: true\n"
                              "t2: True\n"
                              "t3: yes\n"
                              "t4: on\n"
                              "t5: Y\n"
                              "f1: false\n"
                              "f2: FALSE\n"
                              "f3: no\n"
                              "f4: off\n"
                              "f5: n\n"
                              "bad1: 42\n"
                              "bad2: truthy\n");
    ryml::ConstNodeRef root = tree.crootref();

    for (const char *key : {"t1", "t2", "t3", "t4", "t5"})
        CHECK(asBool(root[ryml::to_csubstr(key)]) == true);
    for (const char *key : {"f1", "f2", "f3", "f4", "f5"})
        CHECK(asBool(root[ryml::to_csubstr(key)]) == false);
    CHECK(asBool(root["bad1"]) == std::nullopt);
    CHECK(asBool(root["bad2"]) == std::nullopt);
    CHECK(asBool(child(root, "no_such_key")) == std::nullopt);
}

TEST_CASE("ryml asStringVector: sequences of scalars only", "[yaml][ryml]") {
    ryml::Tree tree = parseOk("strs: [a, b, c]\n"
                              "block:\n"
                              "  - one\n"
                              "  - two\n"
                              "empty: []\n"
                              "nested: [[a], b]\n"
                              "with_null: [a, ~]\n"
                              "scalar: hello\n"
                              "map: {a: 1}\n");
    ryml::ConstNodeRef root = tree.crootref();

    CHECK(asStringVector(root["strs"]) == std::vector<std::string>{"a", "b", "c"});
    CHECK(asStringVector(root["block"]) == std::vector<std::string>{"one", "two"});
    CHECK(asStringVector(root["empty"]) == std::vector<std::string>{});
    // Matches yaml-cpp: non-scalar or null elements fail the whole conversion
    CHECK(asStringVector(root["nested"]) == std::nullopt);
    CHECK(asStringVector(root["with_null"]) == std::nullopt);
    CHECK(asStringVector(root["scalar"]) == std::nullopt);
    CHECK(asStringVector(root["map"]) == std::nullopt);
    CHECK(asStringVector(child(root, "no_such_key")) == std::nullopt);
}

TEST_CASE("ryml child: build-type-independent missing-key lookup", "[yaml][ryml]") {
    ryml::Tree tree = parseOk("manifest:\n  name: lookup\nlist: [a]\nscalar: x\n");
    ryml::ConstNodeRef root = tree.crootref();

    CHECK(child(root, "manifest").readable());
    CHECK(asString(child(child(root, "manifest"), "name")) == "lookup");
    // Missing key: invalid ref, no assert/throw (unlike operator[] in non-NDEBUG builds)
    CHECK_FALSE(child(root, "no_such_key").readable());
    // Non-map nodes can't have keyed children
    CHECK_FALSE(child(root["list"], "x").readable());
    CHECK_FALSE(child(root["scalar"], "x").readable());
    // Chaining through a miss stays safe
    CHECK(asString(child(child(root, "missing"), "deeper")) == std::nullopt);
}

TEST_CASE("ryml emitYaml: emitted text reparses to the same values", "[yaml][ryml]") {
    ryml::Tree tree = parseOk("manifest:\n"
                              "  name: roundtrip\n"
                              "  dirs:\n"
                              "    source: [src, tests]\n"
                              "dependencies:\n"
                              "  - name: fmt\n"
                              "    version: latest\n");

    std::string text = emitYaml(tree);
    ryml::Tree reparsed = parseOk(text.c_str());
    ryml::ConstNodeRef root = reparsed.crootref();

    CHECK(asString(root["manifest"]["name"]) == "roundtrip");
    CHECK(asStringVector(root["manifest"]["dirs"]["source"]) == std::vector<std::string>{"src", "tests"});
    CHECK(asString(root["dependencies"][0]["name"]) == "fmt");
    CHECK(asString(root["dependencies"][0]["version"]) == "latest");
}
