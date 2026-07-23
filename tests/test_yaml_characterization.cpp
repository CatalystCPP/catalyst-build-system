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

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/load_profile_file.hpp"
#include "catalyst/utils/yaml/profile_write_back.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace fs = std::filesystem;
using catalyst::utils::yaml::asString;
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
      FMT: g++
      CC_LAUNCHER: distcc
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
      CC_LAUNCHER: ccache
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
    CHECK(config.getString("manifest.tooling.FMT") == "clang-format");
    CHECK(config.getString("manifest.tooling.LINTER") == "clang-tidy");
    CHECK(config.getString("manifest.tooling.doc.engine") == "doxygen");
    CHECK(config.getString("manifest.dirs.build") == "");
    CHECK(config.getStringVector("manifest.dirs.source") == std::vector<std::string>{});
    namespace yaml = catalyst::utils::yaml;
    CHECK(yaml::child(config.rootRef(), "dependencies").is_seq());
    CHECK(yaml::child(config.rootRef(), "dependencies").num_children() == 0);
    CHECK(yaml::child(config.rootRef(), "features").is_map());
}

TEST_CASE("Profile composition: scalar override, sequence append, dependency append", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_merge"};
    writeFile(dir.path / "CATALYST.yaml", kCompositionYaml);

    Configuration config({"common", "debug"}, dir.path);

    // Later profile overrides scalars
    CHECK(config.getString("manifest.tooling.CC_LAUNCHER") == "ccache");
    CHECK(config.getString("manifest.dirs.build") == "build/debug");
    // Untouched scalars survive
    CHECK(config.getString("manifest.tooling.FMT") == "g++");
    CHECK(config.getString("manifest.name") == "charproj");

    // Sequences append across profiles (defaults start empty)
    CHECK(config.getStringVector("manifest.dirs.source") == std::vector<std::string>{"src", "tests"});
    CHECK(config.getStringVector("manifest.dirs.include") == std::vector<std::string>{"include"});

    // Dependencies append in profile order
    namespace yaml = catalyst::utils::yaml;
    ryml::ConstNodeRef deps = yaml::child(config.rootRef(), "dependencies");
    REQUIRE(deps.readable());
    REQUIRE(deps.is_seq());
    REQUIRE(deps.num_children() == 2);
    CHECK(yaml::asString(yaml::child(deps[0], "name")) == "fmt");
    CHECK(yaml::asString(yaml::child(deps[1], "name")) == "zlib");
}

TEST_CASE("Profile composition: vcpkg transitive booleans survive dependency composition",
          "[yaml][characterization]") {
    TempDir dir{"catalyst_char_vcpkg_transitive"};
    writeFile(dir.path / "CATALYST.yaml",
              R"(common:
  dependencies:
    - name: fmt
      source: vcpkg
      transitive: true
    - name: zlib
      source: vcpkg
      transitive: false
)");

    Configuration config({"common"}, dir.path);

    namespace yaml = catalyst::utils::yaml;
    ryml::ConstNodeRef deps = yaml::child(config.rootRef(), "dependencies");
    REQUIRE(deps.readable());
    REQUIRE(deps.is_seq());
    REQUIRE(deps.num_children() == 2);
    CHECK(yaml::asBool(yaml::child(deps[0], "transitive")) == true);
    CHECK(yaml::asBool(yaml::child(deps[1], "transitive")) == false);
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
        CHECK(config.getStringVector("hooks.pre-build")
              == std::vector<std::string>{"echo common-pre-1", "echo common-pre-2"});
        CHECK(config.getStringVector("hooks.post-build") == std::vector<std::string>{"echo common-post"});
    }
    SECTION("second profile appends its scalar hook and removes via null") {
        Configuration config({"common", "debug"}, dir.path);
        CHECK(config.getStringVector("hooks.pre-build")
              == std::vector<std::string>{"echo common-pre-1", "echo common-pre-2", "echo debug-pre"});
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
    CHECK(config.getStringVector("hooks.pre-build")
          == std::vector<std::string>{"echo common-pre-1", "echo common-pre-2"});
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
    CHECK(config.has("manifest.tooling.FMT"));
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

TEST_CASE("ProfileFile: combined load aliases the profile node into the document", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_wb_combined"};
    writeFile(
        dir.path / "CATALYST.yaml",
        "common:\n  manifest:\n    name: writeback\ndebug:\n  manifest:\n    tooling:\n      CC_LAUNCHER: ccache\n");

    auto loaded = loadProfileFile("debug", dir.path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->path == dir.path / "CATALYST.yaml");

    // Mutating via profile() must be visible through root() (same tree)
    loaded->profile()["manifest"]["version"] = "9.9.9";
    CHECK(asString(loaded->root()["debug"]["manifest"]["version"]) == "9.9.9");

    REQUIRE(profileWriteBack(*loaded).has_value());

    // Reload with yaml-cpp: the written file must stay valid for other parsers
    YAML::Node reloaded = YAML::LoadFile((dir.path / "CATALYST.yaml").string());
    CHECK(reloaded["debug"]["manifest"]["version"].as<std::string>() == "9.9.9");
    CHECK(reloaded["debug"]["manifest"]["tooling"]["CC_LAUNCHER"].as<std::string>() == "ccache");
    // Sibling profiles survive the round trip
    CHECK(reloaded["common"]["manifest"]["name"].as<std::string>() == "writeback");
}

TEST_CASE("ProfileFile: missing profile in combined file is created as an empty map", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_wb_newprof"};
    writeFile(dir.path / "CATALYST.yaml", "common:\n  manifest:\n    name: writeback\n");

    auto loaded = loadProfileFile("release", dir.path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->profile().is_map());
    CHECK(loaded->root()["release"].readable());

    // No operator[] chaining through missing keys in ryml: materialize each level
    ryml::NodeRef manifest = loaded->profile()["manifest"];
    manifest |= ryml::MAP;
    manifest["provides"] = "rel";
    REQUIRE(profileWriteBack(*loaded).has_value());

    YAML::Node reloaded = YAML::LoadFile((dir.path / "CATALYST.yaml").string());
    CHECK(reloaded["release"]["manifest"]["provides"].as<std::string>() == "rel");
    CHECK(reloaded["common"]["manifest"]["name"].as<std::string>() == "writeback");
}

TEST_CASE("ProfileFile: isolated profile file makes profile() the document root", "[yaml][characterization]") {
    TempDir dir{"catalyst_char_wb_isolated"};
    writeFile(dir.path / "catalyst.yaml", "manifest:\n  name: isolated\n");

    auto loaded = loadProfileFile("common", dir.path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->path == dir.path / "catalyst.yaml");

    loaded->profile()["manifest"]["name"] = "changed";
    CHECK(asString(loaded->root()["manifest"]["name"]) == "changed");

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
    CHECK(asString(loaded->profile()["manifest"]["name"]) == "legacy");

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

TEST_CASE("resolveFeatureFlags parses and validates feature flags", "[features][generate]") {
    TempDir dir{"catalyst_char_feature_resolve"};

    constexpr const char *kFeatureTestYaml = R"(common:
  manifest:
    name: testproj
    type: BINARY
    version: 1.0.0
    provides: testproj
    dirs:
      include: [include]
      source: [src]
      build: build
  features:
    simple_bool: true
    obj_bool:
      default: false
      files: [src/opt.cpp]
    log_level:
      type: enum
      values: [off, error, info, debug]
      default: error
    flush_threshold:
      type: int
      default: 1048576
    build_id:
      type: string
      default: "dev-build"
)";

    writeFile(dir.path / "CATALYST.yaml", kFeatureTestYaml);

    SECTION("resolves default values when no overrides are passed") {
        Configuration config({"common"}, dir.path);
        auto res = catalyst::generate::resolveFeatureFlags(config, {});
        REQUIRE(res.has_value());
        const auto &flags = *res;
        REQUIRE(flags.size() == 5);

        auto check_flag =
            [&](const std::string &name, const std::string &type, const std::string &val, bool is_enabled) {
                auto it = std::find_if(flags.begin(), flags.end(), [&](const auto &f) { return f.name == name; });
                REQUIRE(it != flags.end());
                CHECK(it->type == type);
                CHECK(it->resolved_val == val);
                CHECK(it->is_enabled == is_enabled);
            };

        check_flag("simple_bool", "bool", "true", true);
        check_flag("obj_bool", "bool", "false", false);
        check_flag("log_level", "enum", "error", true);
        check_flag("flush_threshold", "int", "1048576", true);
        check_flag("build_id", "string", "dev-build", true);
    }

    SECTION("resolves with CLI overrides") {
        Configuration config({"common"}, dir.path);
        // Toggle bools, override enum and int and string
        std::vector<std::string> cli = {
            "no-simple_bool", "obj_bool", "log_level=debug", "flush_threshold=2097152", "build_id=production-v1"};
        auto res = catalyst::generate::resolveFeatureFlags(config, cli);
        REQUIRE(res.has_value());
        const auto &flags = *res;

        auto check_flag = [&](const std::string &name, const std::string &val, bool is_enabled) {
            auto it = std::find_if(flags.begin(), flags.end(), [&](const auto &f) { return f.name == name; });
            REQUIRE(it != flags.end());
            CHECK(it->resolved_val == val);
            CHECK(it->is_enabled == is_enabled);
        };

        check_flag("simple_bool", "false", false);
        check_flag("obj_bool", "true", true);
        check_flag("log_level", "debug", true);
        check_flag("flush_threshold", "2097152", true);
        check_flag("build_id", "production-v1", true);
    }

    SECTION("validates enum values") {
        Configuration config({"common"}, dir.path);
        // Invalid enum override
        auto res = catalyst::generate::resolveFeatureFlags(config, {"log_level=bogus"});
        CHECK_FALSE(res.has_value());
    }

    SECTION("validates int values") {
        Configuration config({"common"}, dir.path);
        // Invalid int override
        auto res = catalyst::generate::resolveFeatureFlags(config, {"flush_threshold=not_an_int"});
        CHECK_FALSE(res.has_value());
    }

    SECTION("rejects enabling enum/int without a value") {
        Configuration config({"common"}, dir.path);
        auto res = catalyst::generate::resolveFeatureFlags(config, {"log_level"});
        CHECK_FALSE(res.has_value());
    }

    SECTION("rejects disabling enum/int with no- prefix") {
        Configuration config({"common"}, dir.path);
        auto res = catalyst::generate::resolveFeatureFlags(config, {"no-flush_threshold"});
        CHECK_FALSE(res.has_value());
    }
}
