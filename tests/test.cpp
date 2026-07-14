#include <filesystem>
#include <format>
#include <fstream>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/fetch.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/os/os_defs.hpp"
#include "catalyst/utils/yaml/ryml_init.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

TEST_CASE("Basic Check", "[basic]") {
    REQUIRE(1 == 1);
}

TEST_CASE("Generator build filenames", "[generate]") {
    REQUIRE(catalyst::generate::buildFilename("cob") == "catalyst.build");
    REQUIRE(catalyst::generate::buildFilename("ninja") == "build.ninja");
    REQUIRE(catalyst::generate::buildFilename("make") == "Makefile");
    REQUIRE(catalyst::generate::buildFilename("gmake") == "Makefile");
}

class MockProcessExecutor : public catalyst::IProcessExecutor {
public:
    std::vector<std::vector<std::string>> executed_commands;

    std::expected<std::future<int>, std::string>
    processExec(std::vector<std::string> &&args,
                std::optional<std::string> working_dir = std::nullopt,
                std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt,
                bool silent = false) override {
        executed_commands.push_back(args);

        std::promise<int> promise;
        promise.set_value(0);
        return promise.get_future();
    }

    std::expected<std::string, std::string>
    processExecStdout(const std::vector<std::string> &args,
                      const std::optional<std::string> &working_dir = std::nullopt,
                      const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt) override {
        executed_commands.push_back(args);
        return "mocked output";
    }
};

TEST_CASE("Process Executor Mocking", "[process_exec]") {
    auto mock_executor = std::make_shared<MockProcessExecutor>();
    catalyst::setProcessExecutor(mock_executor);

    auto result = catalyst::processExecStdout({"echo", "hello world"});

    REQUIRE(result.has_value());
    REQUIRE(result.value() == "mocked output");
    REQUIRE(mock_executor->executed_commands.size() == 1);
    REQUIRE(mock_executor->executed_commands[0] == std::vector<std::string>{"echo", "hello world"});

    // Restore to prevent side effects on other tests
    catalyst::setProcessExecutor(std::make_shared<catalyst::ProcessExecutor>());
}

TEST_CASE("isCommandInstalled validation", "[os]") {
    // git, vcpkg, pkg-config should be installed in the environment (we verified they exist)
    REQUIRE(catalyst::utils::os::isCommandInstalled("git"));
    REQUIRE(catalyst::utils::os::isCommandInstalled("vcpkg"));
    REQUIRE(catalyst::utils::os::isCommandInstalled("pkg-config"));

    // a non-existent command should not be installed
    REQUIRE_FALSE(catalyst::utils::os::isCommandInstalled("nonexistent_command_xyz123"));
}

TEST_CASE("isCommandInstalled path resolution", "[os]") {
    // Save current PATH
    const char *orig_path = std::getenv("PATH");
    std::string orig_path_str = orig_path ? orig_path : "";

    // Create a temporary directory
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "catalyst_test_path";
    std::filesystem::create_directories(temp_dir);

    // Create a dummy file in it
    std::filesystem::path dummy_exe = temp_dir / "dummy_command";
    {
        std::ofstream dummy_file(dummy_exe);
        dummy_file << "dummy";
    }

    // Set PATH to just this directory
#ifdef _WIN32
    _putenv_s("PATH", temp_dir.string().c_str());
#else
    setenv("PATH", temp_dir.string().c_str(), 1);
#endif

    // Check if it's found
    REQUIRE(catalyst::utils::os::isCommandInstalled("dummy_command"));

    // Restore PATH
#ifdef _WIN32
    _putenv_s("PATH", orig_path_str.c_str());
#else
    if (!orig_path_str.empty()) {
        setenv("PATH", orig_path_str.c_str(), 1);
    } else {
        unsetenv("PATH");
    }
#endif

    // Clean up
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("findConan dependency resolution", "[conan]") {
    namespace fs = std::filesystem;
    fs::path temp_build = fs::temp_directory_path() / "catalyst_test_conan_build";
    fs::create_directories(temp_build);

    fs::path conan_dir = temp_build / "conan";
    fs::create_directories(conan_dir);

    fs::path pc_file = conan_dir / "fmt.pc";
    {
        std::ofstream pc_out(pc_file);
        pc_out << R"(
prefix=/usr
exec_prefix=${prefix}
libdir=${prefix}/lib
includedir=${prefix}/include

Name: fmt
Description: A modern formatting library
Version: 10.1.1
Libs: -L${libdir} -lfmt
Cflags: -I${includedir} -DFMT_HEADER_ONLY
)";
    }

    catalyst::toolchain::ToolchainDef tc;
    tc.flags.include_dir = "-I{path}";
    tc.flags.lib_dir = "-L{path}";
    tc.flags.lib = "-l{name}";

    ryml::Tree dep_tree = catalyst::utils::yaml::parseYaml("name: fmt\nsource: conan\nversion: 10.1.1").value();
    ryml::ConstNodeRef dep = dep_tree.crootref();

    class MockPkgConfigExecutor : public catalyst::IProcessExecutor {
    public:
        std::expected<std::future<int>, std::string>
        processExec(std::vector<std::string> &&args,
                    std::optional<std::string> working_dir = std::nullopt,
                    std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt,
                    bool silent = false) override {
            std::promise<int> promise;
            promise.set_value(0);
            return promise.get_future();
        }

        std::expected<std::string, std::string> processExecStdout(
            const std::vector<std::string> &args,
            const std::optional<std::string> &working_dir = std::nullopt,
            const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt) override {
            if (args.size() >= 3 && args[0] == "pkg-config" && args.back() == "fmt") {
                if (args[1] == "--cflags") {
                    return "-I/usr/include -DFMT_HEADER_ONLY";
                }
                if (args[1] == "--libs-only-L") {
                    return "-L/usr/lib";
                }
                if (args[1] == "--libs-only-l") {
                    return "-lfmt";
                }
            }
            return "";
        }
    };

    auto mock_executor = std::make_shared<MockPkgConfigExecutor>();
    catalyst::setProcessExecutor(mock_executor);

    auto res = catalyst::generate::findConan(temp_build.string(), dep, tc);
    REQUIRE(res.has_value());
    REQUIRE(res->inc_path == " -I/usr/include -DFMT_HEADER_ONLY");
    REQUIRE(res->lib_path == " -L/usr/lib");
    REQUIRE(res->libs == " -lfmt");

    catalyst::setProcessExecutor(std::make_shared<catalyst::ProcessExecutor>());
    fs::remove_all(temp_build);
}

TEST_CASE("fetchConanDeps generates conanfile.txt and executes conan install", "[conan]") {
    namespace fs = std::filesystem;
    fs::path temp_build = fs::temp_directory_path() / "catalyst_test_conan_fetch";
    fs::create_directories(temp_build);

    fs::path temp_root = fs::temp_directory_path() / "catalyst_test_conan_fetch_root";
    fs::create_directories(temp_root);
    fs::path tc_path = temp_root / "tc_test.yaml";
    {
        std::ofstream out(temp_root / "CATALYST.yaml");
        // Absolute toolchain path so parse_toolchain resolves it regardless of CWD.
        out << std::format(R"(
common:
  manifest:
    name: "test_conan"
    toolchain: "{}"
    dirs:
      source: ["src"]
      include: []
      build: "build"
)",
                           tc_path.string());
    }
    {
        // Conan compiler detection reads the toolchain's C++ compiler executable.
        std::ofstream out(tc_path);
        out << R"(
toolchain:
  name: "test"
  compiler:
    cxx:
      executable: "mock_clang++"
)";
    }

    catalyst::utils::yaml::Configuration config({"common"}, temp_root);

    ryml::Tree fmt_tree = catalyst::utils::yaml::parseYaml("name: fmt\nversion: 10.1.1").value();
    ryml::Tree zlib_tree = catalyst::utils::yaml::parseYaml("name: zlib\nversion: 1.2.11").value();
    std::vector<ryml::ConstNodeRef> conan_deps{fmt_tree.crootref(), zlib_tree.crootref()};

    class MockConanInstallExecutor : public catalyst::IProcessExecutor {
    public:
        std::vector<std::vector<std::string>> executed_commands;

        std::expected<std::future<int>, std::string>
        processExec(std::vector<std::string> &&args,
                    std::optional<std::string> working_dir = std::nullopt,
                    std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt,
                    bool silent = false) override {
            executed_commands.push_back(args);
            std::promise<int> promise;
            promise.set_value(0);
            return promise.get_future();
        }

        std::expected<std::string, std::string> processExecStdout(
            const std::vector<std::string> &args,
            const std::optional<std::string> &working_dir = std::nullopt,
            const std::optional<std::unordered_map<std::string, std::string>> &env = std::nullopt) override {
            if (args.size() >= 2 && args[0] == "mock_clang++" && args[1] == "--version") {
                return "mock_clang++ version 17.0.0";
            }
            return "";
        }
    };

    auto mock_executor = std::make_shared<MockConanInstallExecutor>();
    catalyst::setProcessExecutor(mock_executor);

    auto res = catalyst::fetch::fetchConanDeps(conan_deps, temp_build.string(), config, {"common"});
    REQUIRE(res.has_value());

    fs::path conanfile_path = temp_build / "conanfile.txt";
    REQUIRE(fs::exists(conanfile_path));
    std::ifstream conanfile_in(conanfile_path);
    std::stringstream buffer;
    buffer << conanfile_in.rdbuf();
    std::string content = buffer.str();
    REQUIRE(content.find("fmt/10.1.1") != std::string::npos);
    REQUIRE(content.find("zlib/1.2.11") != std::string::npos);
    REQUIRE(content.find("PkgConfigDeps") != std::string::npos);

    REQUIRE(mock_executor->executed_commands.size() == 1);
    const auto &cmd = mock_executor->executed_commands[0];
    REQUIRE(cmd[0] == "conan");
    REQUIRE(cmd[1] == "install");

    bool has_build_type = false;
    bool has_compiler = false;
    bool has_version = false;
    for (size_t i = 0; i < cmd.size(); ++i) {
        if (cmd[i] == "build_type=Release")
            has_build_type = true;
        if (cmd[i] == "compiler=clang")
            has_compiler = true;
        if (cmd[i] == "compiler.version=17")
            has_version = true;
    }
    REQUIRE(has_build_type);
    REQUIRE(has_compiler);
    REQUIRE(has_version);

    catalyst::setProcessExecutor(std::make_shared<catalyst::ProcessExecutor>());
    fs::remove_all(temp_build);
    fs::remove_all(temp_root);
}

TEST_CASE("parseVcpkgStatus extracts installed dependencies", "[vcpkg]") {
    using catalyst::generate::parseVcpkgStatus;
    using catalyst::generate::vcpkgStatusKey;

    // Mirrors the Debian-control layout of $VCPKG_ROOT/installed/vcpkg/status:
    // a base stanza plus a feature stanza for ryml (whose deps are unioned), a
    // not-installed stanza that must be ignored, and a leaf helper port.
    std::string status = R"(Package: c4core
Version: 0.2.6
Depends: vcpkg-cmake, vcpkg-cmake-config
Architecture: x64-linux
Status: install ok installed

Package: ryml
Version: 0.9.0
Depends: c4core, vcpkg-cmake, vcpkg-cmake-config
Architecture: x64-linux
Status: install ok installed

Package: ryml
Feature: extra
Architecture: x64-linux
Depends: zlib [core], openssl (!windows)
Status: install ok installed

Package: removed-pkg
Version: 1.0.0
Depends: should-be-ignored
Architecture: x64-linux
Status: purge ok not-installed

Package: vcpkg-cmake
Version: 2024-04-23
Architecture: x64-linux
Status: install ok installed
)";

    auto db = parseVcpkgStatus(status);

    // Base + feature deps are unioned and de-duplicated, in encounter order.
    // Qualifiers like `[core]` / `(!windows)` are stripped to bare names.
    auto ryml_deps = db.depends.at(vcpkgStatusKey("ryml", "x64-linux"));
    REQUIRE(ryml_deps == std::vector<std::string>{"c4core", "vcpkg-cmake", "vcpkg-cmake-config", "zlib", "openssl"});

    REQUIRE(db.depends.at(vcpkgStatusKey("c4core", "x64-linux"))
            == std::vector<std::string>{"vcpkg-cmake", "vcpkg-cmake-config"});

    // not-installed stanzas are skipped entirely.
    REQUIRE_FALSE(db.depends.contains(vcpkgStatusKey("removed-pkg", "x64-linux")));
}

TEST_CASE("vcpkgTransitiveClosure resolves and orders dependencies", "[vcpkg]") {
    using catalyst::generate::VcpkgStatusDb;
    using catalyst::generate::vcpkgStatusKey;
    using catalyst::generate::vcpkgTransitiveClosure;

    const std::string triplet = "x64-linux";
    auto all_real = [](const std::string &, const std::string &) { return true; };

    SECTION("prunes build-time helper ports and excludes the root") {
        VcpkgStatusDb db;
        db.depends[vcpkgStatusKey("ryml", triplet)] = {"c4core", "vcpkg-cmake", "vcpkg-cmake-config"};
        db.depends[vcpkgStatusKey("c4core", triplet)] = {"vcpkg-cmake", "vcpkg-cmake-config"};

        // Helper ports have no link/include artifacts, so they are filtered out.
        auto is_real = [](const std::string &name, const std::string &) { return !name.starts_with("vcpkg-"); };
        auto closure = vcpkgTransitiveClosure(db, "ryml", triplet, is_real);

        REQUIRE(closure == std::vector<std::string>{"c4core"});
    }

    SECTION("diamond dependencies are emitted once, dependency last") {
        VcpkgStatusDb db;
        db.depends[vcpkgStatusKey("A", triplet)] = {"B", "C"};
        db.depends[vcpkgStatusKey("B", triplet)] = {"D"};
        db.depends[vcpkgStatusKey("C", triplet)] = {"D"};

        auto closure = vcpkgTransitiveClosure(db, "A", triplet, all_real);

        REQUIRE(closure.size() == 3);
        REQUIRE(std::ranges::find(closure, "B") != closure.end());
        REQUIRE(std::ranges::find(closure, "C") != closure.end());
        // D depends on nothing and is depended on by B and C, so it must link last.
        REQUIRE(closure.back() == "D");
        REQUIRE(std::ranges::find(closure, "A") == closure.end());
    }

    SECTION("cycles terminate without duplication") {
        VcpkgStatusDb db;
        db.depends[vcpkgStatusKey("A", triplet)] = {"B"};
        db.depends[vcpkgStatusKey("B", triplet)] = {"A"};

        auto closure = vcpkgTransitiveClosure(db, "A", triplet, all_real);

        REQUIRE(closure == std::vector<std::string>{"B"});
    }

    SECTION("unknown root yields no transitive dependencies") {
        VcpkgStatusDb db;
        auto closure = vcpkgTransitiveClosure(db, "not-installed", triplet, all_real);
        REQUIRE(closure.empty());
    }
}

int main(int argc, char *argv[]) {
    catalyst::utils::yaml::installRymlErrorHandler();
    int result = Catch::Session().run(argc, argv);
    return result;
}
