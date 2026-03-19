#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include "catalyst/process_exec.hpp"

TEST_CASE("Basic Check", "[basic]") {
    REQUIRE(1 == 1);
}

class MockProcessExecutor : public catalyst::IProcessExecutor {
public:
    std::vector<std::vector<std::string>> executed_commands;

    std::expected<std::future<int>, std::string>
    processExec(std::vector<std::string> &&args,
                std::optional<std::string> working_dir = std::nullopt,
                std::optional<std::unordered_map<std::string, std::string>> env = std::nullopt) override {
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

int main(int argc, char* argv[]) {
    int result = Catch::Session().run(argc, argv);
    return result;
}
