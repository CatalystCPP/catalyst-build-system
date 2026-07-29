#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/build.hpp"
#include "catalyst/workspace.hpp"

// Catch2 assertion/test-case macros expand to do-while loops and large generated functions.
// NOLINTBEGIN(cppcoreguidelines-avoid-do-while, readability-function-cognitive-complexity)

namespace {
namespace fs = std::filesystem;
using namespace std::chrono_literals;

class TemporaryWorkspace {
public:
    TemporaryWorkspace(const TemporaryWorkspace &) = delete;
    TemporaryWorkspace(TemporaryWorkspace &&) = delete;
    TemporaryWorkspace &operator=(const TemporaryWorkspace &) = delete;
    TemporaryWorkspace &operator=(TemporaryWorkspace &&) = delete;

    TemporaryWorkspace() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        root_path = fs::temp_directory_path() / std::format("catalyst_workspace_build_{}", suffix);
        fs::create_directories(root_path);
    }

    ~TemporaryWorkspace() {
        std::error_code error;
        fs::remove_all(root_path, error);
    }

    [[nodiscard]] const fs::path &root() const {
        return root_path;
    }

private:
    fs::path root_path;
};

class ProcessExecutorGuard {
public:
    explicit ProcessExecutorGuard(std::shared_ptr<catalyst::IProcessExecutor> executor) {
        catalyst::setProcessExecutor(std::move(executor));
    }

    ProcessExecutorGuard(const ProcessExecutorGuard &) = delete;
    ProcessExecutorGuard(ProcessExecutorGuard &&) = delete;
    ProcessExecutorGuard &operator=(const ProcessExecutorGuard &) = delete;
    ProcessExecutorGuard &operator=(ProcessExecutorGuard &&) = delete;

    ~ProcessExecutorGuard() {
        catalyst::setProcessExecutor(std::make_shared<catalyst::ProcessExecutor>());
    }
};

std::future<int> readyProcess(int exit_code) {
    std::promise<int> promise;
    promise.set_value(exit_code);
    return promise.get_future();
}

void writeFile(const fs::path &path, const std::string &contents) {
    std::ofstream output{path};
    REQUIRE(output.is_open());
    output << contents;
    REQUIRE(output.good());
}

void writeMember(const fs::path &workspace_root,
                 const std::string &member_name,
                 const std::vector<std::string> &dependencies = {}) {
    const fs::path member_path = workspace_root / member_name;
    fs::create_directories(member_path);

    std::string manifest = std::format("common:\n"
                                       "  manifest:\n"
                                       "    name: {}\n"
                                       "    type: INTERFACE\n"
                                       "    dirs:\n"
                                       "      source: []\n"
                                       "      include: []\n"
                                       "      build: build\n",
                                       member_name);
    if (!dependencies.empty()) {
        manifest += "  dependencies:\n";
        for (const auto &dependency : dependencies) {
            manifest += std::format("    - name: {}\n"
                                    "      source: local\n"
                                    "      path: ../{}\n",
                                    dependency,
                                    dependency);
        }
    }
    writeFile(member_path / "CATALYST.yaml", manifest);
}

std::optional<catalyst::Workspace>
writeWorkspace(const fs::path &root, const std::vector<std::pair<std::string, std::vector<std::string>>> &members) {
    std::string workspace_manifest;
    for (const auto &[member_name, dependencies] : members) {
        workspace_manifest += std::format("{}:\n"
                                          "  path: {}\n"
                                          "  profiles: [common]\n",
                                          member_name,
                                          member_name);
        writeMember(root, member_name, dependencies);
    }
    writeFile(root / "WORKSPACE.yaml", workspace_manifest);
    return catalyst::Workspace::findRoot(root);
}

catalyst::build::Parse buildArgs(const catalyst::Workspace &workspace) {
    return {
        .regen = false,
        .force_rebuild = false,
        .force_refetch = false,
        .workspace_build = true,
        .watch = false,
        .package = "",
        .profiles = {"common"},
        .enabled_features = {},
        .backend = "",
        .workspace = workspace,
        .executable_path = "catalyst-test",
    };
}

class WorkspaceProcessExecutor : public catalyst::IProcessExecutor {
public:
    enum class Behavior : std::uint8_t { ParallelBarrier, DependencyOrder, FailureWait };

    explicit WorkspaceProcessExecutor(Behavior selected_behavior) : behavior(selected_behavior) {
    }

    catalyst::Result<std::future<int>>
    processExec(std::vector<std::string> &&args,
                std::optional<std::string> working_dir,
                std::optional<std::unordered_map<std::string, std::string>> environment,
                bool silent) override {
        static_cast<void>(std::move(args));
        static_cast<void>(environment);
        static_cast<void>(silent);
        if (!working_dir)
            return std::unexpected("Workspace build did not provide a working directory.");
        const std::string member_name = fs::path(*working_dir).filename().string();

        {
            std::lock_guard lock{mutex};
            working_directories.insert(fs::path(*working_dir));
        }

        switch (behavior) {
            case Behavior::ParallelBarrier:
                return parallelBarrier(member_name);
            case Behavior::DependencyOrder:
                return dependencyOrder(member_name);
            case Behavior::FailureWait:
                return failureWait(member_name);
        }
        return std::unexpected("Unknown workspace process-executor behavior.");
    }

    catalyst::Result<std::string>
    processExecStdout(const std::vector<std::string> &args,
                      const std::optional<std::string> &working_dir,
                      const std::optional<std::unordered_map<std::string, std::string>> &environment) override {
        static_cast<void>(args);
        static_cast<void>(working_dir);
        static_cast<void>(environment);
        return std::unexpected("Unexpected stdout process execution.");
    }

    [[nodiscard]] bool slowBuildCompleted() const {
        std::lock_guard lock{mutex};
        return slow_build_completed;
    }

    [[nodiscard]] std::unordered_set<fs::path> workingDirectories() const {
        std::lock_guard lock{mutex};
        return working_directories;
    }

private:
    catalyst::Result<std::future<int>> parallelBarrier(const std::string &member_name) {
        std::unique_lock lock{mutex};
        started_members.insert(member_name);
        condition.notify_all();
        const bool both_started = condition.wait_for(lock, 2s, [this]() { return started_members.size() == 2; });
        return readyProcess(both_started ? 0 : 1);
    }

    catalyst::Result<std::future<int>> dependencyOrder(const std::string &member_name) {
        std::lock_guard lock{mutex};
        if (member_name == "library") {
            library_completed = true;
            return readyProcess(0);
        }
        return readyProcess(library_completed ? 0 : 1);
    }

    catalyst::Result<std::future<int>> failureWait(const std::string &member_name) {
        if (member_name == "failing")
            return readyProcess(1);

        std::this_thread::sleep_for(100ms);
        {
            std::lock_guard lock{mutex};
            slow_build_completed = true;
        }
        return readyProcess(0);
    }

    Behavior behavior;
    mutable std::mutex mutex;
    std::condition_variable condition;
    std::unordered_set<std::string> started_members;
    std::unordered_set<fs::path> working_directories;
    bool library_completed{false};
    bool slow_build_completed{false};
};

} // namespace

TEST_CASE("Independent workspace builds run concurrently with explicit working directories", "[build][workspace]") {
    TemporaryWorkspace temporary_workspace;
    auto workspace = writeWorkspace(temporary_workspace.root(), {{"left", {}}, {"right", {}}});
    REQUIRE(workspace.has_value());

    auto executor = std::make_shared<WorkspaceProcessExecutor>(WorkspaceProcessExecutor::Behavior::ParallelBarrier);
    ProcessExecutorGuard executor_guard{executor};
    const fs::path original_working_directory = fs::current_path();

    auto result = catalyst::build::action(buildArgs(*workspace));

    REQUIRE(result.has_value());
    REQUIRE(fs::current_path() == original_working_directory);
    REQUIRE(executor->workingDirectories()
            == std::unordered_set<fs::path>{temporary_workspace.root() / "left", temporary_workspace.root() / "right"});
}

TEST_CASE("Workspace dependencies complete before their dependents start", "[build][workspace]") {
    TemporaryWorkspace temporary_workspace;
    auto workspace =
        writeWorkspace(temporary_workspace.root(), {{"library", {}}, {"application", {"library"}}, {"unrelated", {}}});
    REQUIRE(workspace.has_value());

    auto executor = std::make_shared<WorkspaceProcessExecutor>(WorkspaceProcessExecutor::Behavior::DependencyOrder);
    ProcessExecutorGuard executor_guard{executor};
    auto args = buildArgs(*workspace);
    args.package = "application";

    REQUIRE(catalyst::build::action(args).has_value());
    REQUIRE(executor->workingDirectories()
            == std::unordered_set<fs::path>{temporary_workspace.root() / "library",
                                            temporary_workspace.root() / "application"});
}

TEST_CASE("Workspace builds wait for every dispatched task before reporting failure", "[build][workspace]") {
    TemporaryWorkspace temporary_workspace;
    auto workspace = writeWorkspace(temporary_workspace.root(), {{"failing", {}}, {"slow", {}}});
    REQUIRE(workspace.has_value());

    auto executor = std::make_shared<WorkspaceProcessExecutor>(WorkspaceProcessExecutor::Behavior::FailureWait);
    ProcessExecutorGuard executor_guard{executor};

    auto result = catalyst::build::action(buildArgs(*workspace));

    REQUIRE_FALSE(result.has_value());
    REQUIRE(executor->slowBuildCompleted());
}

TEST_CASE("Invalid workspace build graphs fail before dispatch", "[build][workspace]") {
    SECTION("Circular dependencies") {
        TemporaryWorkspace temporary_workspace;
        auto workspace = writeWorkspace(temporary_workspace.root(), {{"left", {"right"}}, {"right", {"left"}}});
        REQUIRE(workspace.has_value());

        auto executor = std::make_shared<WorkspaceProcessExecutor>(WorkspaceProcessExecutor::Behavior::DependencyOrder);
        ProcessExecutorGuard executor_guard{executor};

        auto result = catalyst::build::action(buildArgs(*workspace));

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().find("Circular workspace dependency") != std::string::npos);
        REQUIRE(executor->workingDirectories().empty());
    }

    SECTION("Invalid member configuration") {
        TemporaryWorkspace temporary_workspace;
        auto workspace = writeWorkspace(temporary_workspace.root(), {{"member", {}}});
        REQUIRE(workspace.has_value());
        writeFile(temporary_workspace.root() / "WORKSPACE.yaml",
                  "member:\n"
                  "  path: member\n"
                  "  profiles: [missing]\n");
        workspace = catalyst::Workspace::findRoot(temporary_workspace.root());
        REQUIRE(workspace.has_value());

        auto executor = std::make_shared<WorkspaceProcessExecutor>(WorkspaceProcessExecutor::Behavior::DependencyOrder);
        ProcessExecutorGuard executor_guard{executor};

        auto result = catalyst::build::action(buildArgs(*workspace));

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().find("Failed to load config") != std::string::npos);
        REQUIRE(executor->workingDirectories().empty());
    }
}

TEST_CASE("Workspace watch mode is rejected before dispatch", "[build][workspace]") {
    TemporaryWorkspace temporary_workspace;
    auto workspace = writeWorkspace(temporary_workspace.root(), {{"member", {}}});
    REQUIRE(workspace.has_value());

    auto executor = std::make_shared<WorkspaceProcessExecutor>(WorkspaceProcessExecutor::Behavior::DependencyOrder);
    ProcessExecutorGuard executor_guard{executor};
    auto args = buildArgs(*workspace);
    args.watch = true;

    auto result = catalyst::build::action(args);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().find("do not support --watch") != std::string::npos);
    REQUIRE(executor->workingDirectories().empty());
}

// NOLINTEND(cppcoreguidelines-avoid-do-while, readability-function-cognitive-complexity)
