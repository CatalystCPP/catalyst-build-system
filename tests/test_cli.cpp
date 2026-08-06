#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "catalyst/dispatch.hpp"

// Catch2 assertion macros expand to do-while loops.
// NOLINTBEGIN(cppcoreguidelines-avoid-do-while)
TEST_CASE("Test passthrough preserves arguments after the separator", "[cli][test]") {
    std::vector<std::string> arguments{
        "catalyst",
        "test",
        "--rebuild",
        "--",
        "[toolchain]",
        "--reporter=compact",
        "value with spaces",
        "comma,value",
        R"(quoted "value")",
    };
    std::vector<char *> argv;
    argv.reserve(arguments.size());
    for (std::string &argument : arguments) {
        argv.push_back(argument.data());
    }

    catalyst::CliContext context;
    const auto [exit_code, should_return] = catalyst::parseCli(static_cast<int>(argv.size()), argv.data(), context);

    CHECK(exit_code == 0);
    CHECK_FALSE(should_return);
    REQUIRE(context.test_res != nullptr);
    CHECK(context.test_res->rebuild);
    CHECK(context.test_res->params
          == std::vector<std::string>{
              "[toolchain]",
              "--reporter=compact",
              "value with spaces",
              "comma,value",
              R"(quoted "value")",
          });
}

TEST_CASE("Legacy test params remain supported", "[cli][test]") {
    std::vector<std::string> arguments{
        "catalyst",
        "test",
        "--params",
        "legacy-param",
    };
    std::vector<char *> argv;
    argv.reserve(arguments.size());
    for (std::string &argument : arguments) {
        argv.push_back(argument.data());
    }

    catalyst::CliContext context;
    const auto [exit_code, should_return] = catalyst::parseCli(static_cast<int>(argv.size()), argv.data(), context);

    CHECK(exit_code == 0);
    CHECK_FALSE(should_return);
    REQUIRE(context.test_res != nullptr);
    CHECK(context.test_res->params == std::vector<std::string>{"legacy-param"});
}

TEST_CASE("Run passthrough preserves arguments after the separator", "[cli][run]") {
    std::vector<std::string> arguments{
        "catalyst",
        "run",
        "--profiles",
        "common",
        "--",
        "[application]",
        "--verbose",
        "value with spaces",
        "comma,value",
        R"(quoted "value")",
    };
    std::vector<char *> argv;
    argv.reserve(arguments.size());
    for (std::string &argument : arguments) {
        argv.push_back(argument.data());
    }

    catalyst::CliContext context;
    const auto [exit_code, should_return] = catalyst::parseCli(static_cast<int>(argv.size()), argv.data(), context);

    CHECK(exit_code == 0);
    CHECK_FALSE(should_return);
    REQUIRE(context.run_res != nullptr);
    CHECK(context.run_res->params
          == std::vector<std::string>{
              "[application]",
              "--verbose",
              "value with spaces",
              "comma,value",
              R"(quoted "value")",
          });
}

TEST_CASE("Bench passthrough preserves arguments after the separator", "[cli][bench]") {
    std::vector<std::string> arguments{
        "catalyst",
        "bench",
        "--rebuild",
        "--",
        "[benchmark]",
        "--benchmark_filter=fast.*",
        "value with spaces",
        "comma,value",
        R"(quoted "value")",
    };
    std::vector<char *> argv;
    argv.reserve(arguments.size());
    for (std::string &argument : arguments) {
        argv.push_back(argument.data());
    }

    catalyst::CliContext context;
    const auto [exit_code, should_return] = catalyst::parseCli(static_cast<int>(argv.size()), argv.data(), context);

    CHECK(exit_code == 0);
    CHECK_FALSE(should_return);
    REQUIRE(context.bench_res != nullptr);
    CHECK(context.bench_res->rebuild);
    CHECK(context.bench_res->params
          == std::vector<std::string>{
              "[benchmark]",
              "--benchmark_filter=fast.*",
              "value with spaces",
              "comma,value",
              R"(quoted "value")",
          });
}
// NOLINTEND(cppcoreguidelines-avoid-do-while)
