#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <fstream>
#include <filesystem>

#include "catalyst/process_exec.hpp"
#include "catalyst/utils/os/os_defs.hpp"

TEST_CASE("Basic Check", "[basic]") {
    REQUIRE(1 == 1);
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


int main(int argc, char* argv[]) {
    int result = Catch::Session().run(argc, argv);
    return result;
}
