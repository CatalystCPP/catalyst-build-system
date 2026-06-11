// Characterization tests for the YAML profile-composition and write-back
// behavior, written against the current yaml-cpp implementation. These pin
// down the semantics the rapidyaml port must preserve (see
// docs/yaml-cpp-to-rapidyaml-port.md, Phase 0). If one of these breaks during
// the port, the port changed observable behavior — not the test.

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <ryml/ryml.hpp>
#include <yaml-cpp/yaml.h>

#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"

namespace fs = std::filesystem;
using catalyst::utils::yaml::Configuration;
using catalyst::utils::yaml::loadProfileFile;
using catalyst::utils::yaml::profileWriteBack;

namespace {

struct TempDir {
    fs::path path;
    explicit TempDir(const std::string &name) : path(fs::temp_directory_path() / name) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        fs::remove_all(path);
    }
    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;
};

void writeFile(const fs::path &p, const std::string &content) {
    std::ofstream out{p};
    out << content;
}

constexpr const char *kCompositionYaml = R"(common:
  meta:
    min_ver: 1.0.0
    generator: ninja
  manifest:
    name: charproj
    type: BINARY
    version: 1.2.3
    provides: charproj
    tooling:
      CXX: g++
      CXXFLAGS: -O2
    dirs:
      include: [include]
      source: [src]
      build: build
  dependencies:
    - name: fmt
      source: vcpkg
      triplet: x64-linux
      version: latest
  features:
    uniform_logs: false
    answer: 42
  hooks:
    pre-build:
      - echo common-pre-1
      - echo common-pre-2
    post-build:
      - echo common-post
debug:
  meta:
    min_ver: 0.5.0
  manifest:
    tooling:
      CXXFLAGS: -O0 -g
    dirs:
      source: [tests]
      build: build/debug
  dependencies:
    - name: zlib
      source: vcpkg
      triplet: x64-linux
      version: latest
  features:
    - log_machine_info: true
  hooks:
    pre-build: echo debug-pre
    post-build: null
newer:
  meta:
    min_ver: 2.0.0
removal:
  manifest:
    provides: null
badgen:
  meta:
    generator: frobnicate
nullgen:
  meta:
    generator: null
invalid:
  bogus: 1
)";

} // namespace

TEST_CASE("Profile composition: defaults applied under a minimal profile", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_defaults"};
    writeFile(dir.path / "CATALYST.yaml", "common:\n  manifest:\n    name: minimal\n");

    Configuration config({"common"}, dir.path);

    // Values from the profile
    CHECK(config.getString("manifest.name") == "minimal");
    // Values from getDefaultConfiguration()
    CHECK(config.getString("meta.min_ver") == "0.0.1");
    CHECK(config.getString("meta.generator") == "cob");
    CHECK(config.getString("manifest.type") == "BINARY");
    CHECK(config.getString("manifest.version") == "0.0.1");
    CHECK(config.getString("manifest.provides") == "");
    CHECK(config.getString("manifest.tooling.CC") == "clang");
    CHECK(config.getString("manifest.tooling.CXX") == "clang++");
    CHECK(config.getString("manifest.tooling.FMT") == "clang-format");
    CHECK(config.getString("manifest.tooling.LINTER") == "clang-tidy");
    CHECK(config.getString("manifest.tooling.doc.engine") == "doxygen");
    CHECK(config.getString("manifest.dirs.build") == "");
    CHECK(config.getStringVector("manifest.dirs.source") == std::vector<std::string>{});
    CHECK(config.getRoot()["dependencies"].IsSequence());
    CHECK(config.getRoot()["dependencies"].size() == 0);
    CHECK(config.getRoot()["features"].IsMap());
}

TEST_CASE("Profile composition: scalar override, sequence append, dependency append", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_merge"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    Configuration config({"common", "debug"}, dir.path);

    // Later profile overrides scalars
    CHECK(config.getString("manifest.tooling.CXXFLAGS") == "-O0 -g");
    CHECK(config.getString("manifest.dirs.build") == "build/debug");
    // Untouched scalars survive
    CHECK(config.getString("manifest.tooling.CXX") == "g++");
    CHECK(config.getString("manifest.name") == "charproj");

    // Sequences append across profiles (defaults start empty)
    CHECK(config.getStringVector("manifest.dirs.source") == std::vector<std::string>{"src", "tests"});
    CHECK(config.getStringVector("manifest.dirs.include") == std::vector<std::string>{"include"});

    // Dependencies append in profile order
    const YAML::Node &deps = config.getRoot()["dependencies"];
    REQUIRE(deps.IsSequence());
    REQUIRE(deps.size() == 2);
    CHECK(deps[0]["name"].as<std::string>() == "fmt");
    CHECK(deps[1]["name"].as<std::string>() == "zlib");
}

TEST_CASE("Profile composition: min_ver takes the maximum version seen", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_minver"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    SECTION("lower incoming min_ver is ignored") {
        Configuration config({"common", "debug"}, dir.path);
        CHECK(config.getString("meta.min_ver") == "1.0.0");
    }
    SECTION("higher incoming min_ver wins") {
        Configuration config({"common", "newer"}, dir.path);
        CHECK(config.getString("meta.min_ver") == "2.0.0");
    }
}

TEST_CASE("Profile composition: null value removes the key", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_null"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    Configuration base({"common"}, dir.path);
    REQUIRE(base.getString("manifest.provides") == "charproj");

    Configuration config({"common", "removal"}, dir.path);
    CHECK_FALSE(config.has("manifest.provides"));
    CHECK(config.getString("manifest.provides") == std::nullopt);
}

TEST_CASE("Profile composition: generator is validated", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_generator"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    SECTION("valid value applies") {
        Configuration config({"common"}, dir.path);
        CHECK(config.getString("meta.generator") == "ninja");
    }
    SECTION("invalid value is ignored, previous value kept") {
        Configuration config({"common", "badgen"}, dir.path);
        CHECK(config.getString("meta.generator") == "ninja");
    }
    SECTION("null resets to the 'cob' fallback") {
        Configuration config({"common", "nullgen"}, dir.path);
        CHECK(config.getString("meta.generator") == "cob");
    }
}

TEST_CASE("Profile composition: features merge from map and sequence-of-maps forms", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_features"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    Configuration config({"common", "debug"}, dir.path);

    CHECK(config.getBool("features.uniform_logs") == false);
    CHECK(config.getBool("features.log_machine_info") == true);
    // Scalars coerce across types through the getters
    CHECK(config.getInt("features.answer") == 42);
    CHECK(config.getString("features.answer") == "42");
}

TEST_CASE("Profile composition: hooks append, scalars wrap, null removes", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_hooks"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    SECTION("single profile") {
        Configuration config({"common"}, dir.path);
        CHECK(config.getStringVector("hooks.pre-build") ==
              std::vector<std::string>{"echo common-pre-1", "echo common-pre-2"});
        CHECK(config.getStringVector("hooks.post-build") == std::vector<std::string>{"echo common-post"});
    }
    SECTION("second profile appends its scalar hook and removes via null") {
        Configuration config({"common", "debug"}, dir.path);
        CHECK(config.getStringVector("hooks.pre-build") ==
              std::vector<std::string>{"echo common-pre-1", "echo common-pre-2", "echo debug-pre"});
        CHECK_FALSE(config.has("hooks.post-build"));
    }
}

TEST_CASE("Profile composition: invalid profile keys throw", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_invalid"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    CHECK_THROWS_AS(Configuration({"common", "invalid"}, dir.path), std::runtime_error);
}

TEST_CASE("Profile composition: missing profile throws", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_missing"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    CHECK_THROWS(Configuration({"common", "no_such_profile"}, dir.path));
}

TEST_CASE("Profile composition: adjacent duplicate profiles are applied once", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_dup"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    Configuration config({"common", "common"}, dir.path);
    CHECK(config.getStringVector("manifest.dirs.source") == std::vector<std::string>{"src"});
    CHECK(config.getStringVector("hooks.pre-build") ==
          std::vector<std::string>{"echo common-pre-1", "echo common-pre-2"});
}

TEST_CASE("Configuration getters: type mismatches yield nullopt", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_getters"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    Configuration config({"common"}, dir.path);

    CHECK(config.getInt("manifest.name") == std::nullopt);
    CHECK(config.getBool("manifest.name") == std::nullopt);
    CHECK(config.getStringVector("manifest.name") == std::nullopt);
    // Map and sequence nodes can't convert to scalars
    CHECK(config.getString("manifest.tooling") == std::nullopt);
    CHECK(config.getString("manifest.dirs.source") == std::nullopt);
    // Missing keys
    CHECK(config.getString("manifest.no_such_key") == std::nullopt);
    CHECK(config.getString("no.such.path") == std::nullopt);
    CHECK_FALSE(config.has("manifest.no_such_key"));
    CHECK(config.has("manifest.tooling.CXX"));
}

TEST_CASE("Configuration::getBuildDir multiplexes the profile list", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_builddir"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    SECTION("single profile") {
        Configuration config({"common"}, dir.path);
        CHECK(config.getBuildDir() == fs::path{"build"} / "common");
    }
    SECTION("multiple profiles join with '-'") {
        Configuration config({"common", "debug"}, dir.path);
        CHECK(config.getBuildDir() == fs::path{"build/debug"} / "common-debug");
    }
}

TEST_CASE("ProfileFile: combined load aliases profile_node into root_node", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_wb_combined"};
    writeFile(dir.path / "CATALYST.yaml",
              "common:\n  manifest:\n    name: writeback\ndebug:\n  manifest:\n    tooling:\n      CXXFLAGS: -g\n");

    auto loaded = loadProfileFile("debug", dir.path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->path == dir.path / "CATALYST.yaml");

    // Mutating profile_node must be visible through root_node (shared state)
    loaded->profile_node["manifest"]["version"] = "9.9.9";
    CHECK(loaded->root_node["debug"]["manifest"]["version"].as<std::string>() == "9.9.9");

    REQUIRE(profileWriteBack(*loaded).has_value());

    YAML::Node reloaded = YAML::LoadFile((dir.path / "CATALYST.yaml").string());
    CHECK(reloaded["debug"]["manifest"]["version"].as<std::string>() == "9.9.9");
    CHECK(reloaded["debug"]["manifest"]["tooling"]["CXXFLAGS"].as<std::string>() == "-g");
    // Sibling profiles survive the round trip
    CHECK(reloaded["common"]["manifest"]["name"].as<std::string>() == "writeback");
}

TEST_CASE("ProfileFile: missing profile in combined file is created as an empty map", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_wb_newprof"};
    writeFile(dir.path / "CATALYST.yaml", "common:\n  manifest:\n    name: writeback\n");

    auto loaded = loadProfileFile("release", dir.path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->profile_node.IsMap());
    CHECK(loaded->root_node["release"].IsDefined());

    loaded->profile_node["manifest"]["provides"] = "rel";
    REQUIRE(profileWriteBack(*loaded).has_value());

    YAML::Node reloaded = YAML::LoadFile((dir.path / "CATALYST.yaml").string());
    CHECK(reloaded["release"]["manifest"]["provides"].as<std::string>() == "rel");
    CHECK(reloaded["common"]["manifest"]["name"].as<std::string>() == "writeback");
}

TEST_CASE("ProfileFile: isolated profile file makes root_node and profile_node the same document",
          "[yaml][characterization]") {
    TempDir dir{"catalyst_char_wb_isolated"};
    writeFile(dir.path / "catalyst.yaml", "manifest:\n  name: isolated\n");

    auto loaded = loadProfileFile("common", dir.path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->path == dir.path / "catalyst.yaml");

    loaded->profile_node["manifest"]["name"] = "changed";
    CHECK(loaded->root_node["manifest"]["name"].as<std::string>() == "changed");

    REQUIRE(profileWriteBack(*loaded).has_value());

    // The isolated file has no profile-name nesting
    YAML::Node reloaded = YAML::LoadFile((dir.path / "catalyst.yaml").string());
    CHECK(reloaded["manifest"]["name"].as<std::string>() == "changed");
}

TEST_CASE("ProfileFile: missing isolated profile file is an error", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_wb_nofile"};

    auto loaded = loadProfileFile("common", dir.path);
    CHECK_FALSE(loaded.has_value());
}

TEST_CASE("ProfileFile: legacy isolated file is merged into CATALYST.yaml on load, not deleted",
          "[yaml][characterization]") {
    TempDir dir{"catalyst_char_wb_legacy"};
    writeFile(dir.path / "CATALYST.yaml", "common:\n  manifest:\n    name: writeback\n");
    writeFile(dir.path / "catalyst_debug.yaml", "manifest:\n  name: legacy\n");

    auto loaded = loadProfileFile("debug", dir.path);
    REQUIRE(loaded.has_value());
    // The isolated file's content wins and the combined file is the write target
    CHECK(loaded->path == dir.path / "CATALYST.yaml");
    CHECK(loaded->profile_node["manifest"]["name"].as<std::string>() == "legacy");

    REQUIRE(profileWriteBack(*loaded).has_value());

    YAML::Node reloaded = YAML::LoadFile((dir.path / "CATALYST.yaml").string());
    CHECK(reloaded["debug"]["manifest"]["name"].as<std::string>() == "legacy");
    // The legacy file is preserved to avoid data loss
    CHECK(fs::exists(dir.path / "catalyst_debug.yaml"));
}

// Phase 0 sanity check: installRymlErrorHandler() (called in this binary's
// main) must turn rapidyaml's default abort-on-error into a catchable
// exception, or every parse error in the ported code would kill the process.
TEST_CASE("ryml error handler: parse errors throw instead of aborting", "[yaml][ryml]") {
    CHECK_THROWS_AS(ryml::parse_in_arena("a: [unclosed"), std::runtime_error);

    ryml::Tree tree = ryml::parse_in_arena("a: 1");
    CHECK(tree["a"].val() == "1");
}

TEST_CASE("ProfileFile: malformed YAML reports a parse error", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_wb_malformed"};
    writeFile(dir.path / "CATALYST.yaml", "common:\n  manifest: [unclosed\n");

    auto loaded = loadProfileFile("common", dir.path);
    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().find("Failed to parse YAML file") != std::string::npos);
}
