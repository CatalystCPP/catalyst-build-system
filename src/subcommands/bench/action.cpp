#include <algorithm>
#include <cctype>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <tuple>
#include <vector>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/bench.hpp"
#include "catalyst/subcommands/build.hpp"
#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/toolchain.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace fs = std::filesystem;

namespace catalyst::bench {

namespace {
std::string commandStr(const fs::path &executable, const std::vector<std::string> &params) {
    catalyst::logger.debug("Constructing command string.");
    std::string command = executable;
    for (const auto &param : params) {
        command += " " + param;
    }
    return command;
}

// Warns (without failing) if any build input is newer than the benchmark executable, hinting that it may be stale.
void warnIfStale(const fs::path &exe_path,
                 const utils::yaml::Configuration &profile_comp,
                 const std::vector<std::string> &profiles) {
    if (!fs::exists(exe_path)) {
        return;
    }

    std::error_code exe_ec;
    fs::file_time_type exe_mtime = fs::last_write_time(exe_path, exe_ec);
    if (exe_ec) {
        catalyst::logger.debug("Could not read mtime of benchmark executable: {}", exe_path.string());
        return;
    }

    std::vector<fs::path> inputs;

    std::vector<std::string> source_dirs =
        profile_comp.getStringVector("manifest.dirs.source").value_or(std::vector<std::string>{"src"});
    if (auto source_set = generate::buildSourceSet(source_dirs, profiles); source_set) {
        inputs.insert(inputs.end(), source_set->begin(), source_set->end());
    } else {
        catalyst::logger.debug("Skipping source set in staleness check, failed to build: {}", source_set.error());
    }

    catalyst::toolchain::ToolchainDef tc;
    if (auto toolchain_path = profile_comp.getString("manifest.toolchain")) {
        if (auto parsed = catalyst::toolchain::parseToolchain(*toolchain_path)) {
            tc = std::move(*parsed);
        }
    }

    std::vector<std::string> include_dirs =
        profile_comp.getStringVector("manifest.dirs.include").value_or(std::vector<std::string>{"include"});
    for (const auto &inc : include_dirs) {
        fs::path inc_dir{inc};
        if (!fs::exists(inc_dir) || !fs::is_directory(inc_dir)) {
            continue;
        }
        std::error_code walk_ec;
        for (const auto &entry : fs::recursive_directory_iterator(inc_dir, walk_ec)) {
            if (entry.is_regular_file()
                && std::ranges::contains(tc.extensions.headers, entry.path().extension().string())) {
                inputs.push_back(entry.path());
            }
        }
    }

    bool stale = false;
    for (const auto &input : inputs) {
        std::error_code in_ec;
        fs::file_time_type input_mtime = fs::last_write_time(input, in_ec);
        if (!in_ec && input_mtime > exe_mtime) {
            catalyst::logger.warn("Input file is newer than benchmark executable: {}", input.string());
            stale = true;
        }
    }
    if (stale) {
        catalyst::logger.warn("Benchmark executable may be stale. Consider rebuilding before benchmarking.");
    }
}
} // namespace

Result<void> action(const Parse &args) {
    catalyst::logger.debug("Bench subcommand invoked.");

    if (args.workspace) {
        fs::path current = fs::current_path();
        bool is_root = false;
        try {
            is_root = fs::equivalent(args.workspace->getRoot(), current);
        } catch (...) {
            std::ignore;
        }

        if (is_root) {
            catalyst::logger.info("Running benchmarks for all workspace members.");
            bool any_failed = false;
            std::string failed_members;

            for (const auto &[name, member] : args.workspace->getMembers()) {
                catalyst::logger.info("Benchmarking member: {}", name);
                fs::current_path(member.path);

                Parse member_args = args;
                member_args.workspace = std::nullopt; // Prevent recursion loop

                if (auto res = action(member_args); !res) {
                    catalyst::logger.error("Benchmark failed for member: {}", name);
                    any_failed = true;
                    failed_members += name + " ";
                }
                fs::current_path(current);
            }
            if (any_failed) {
                return std::unexpected("Benchmarks failed for members: " + failed_members);
            }
            return {};
        }
    }

    std::vector<std::string> profiles{"common", "bench"};

    catalyst::logger.debug("Composing profiles.");
    auto res = generate::profileComposition(profiles);
    if (!res) {
        return std::unexpected(std::format("Faield to compose profiles: {}", res.error()));
    }
    const utils::yaml::Configuration &profile_comp = res.value();

    catalyst::logger.debug("Running pre-bench hooks.");
    if (auto hook_res = hooks::preBench(profile_comp); !hook_res) {
        return hook_res;
    }

    std::string exe;
    std::string build_dir;
    auto str_to_lower = [](std::string &input) -> void {
        auto lower = [](const char c) -> char { return static_cast<char>(std::tolower(c)); };
        std::ranges::transform(input, input.begin(), lower);
        return;
    };
    std::optional<std::string> type_opt = profile_comp.getString("manifest.type");
    std::string target_type = type_opt.value_or("");
    str_to_lower(target_type);

    if (!type_opt || target_type != "binary") {
        return std::unexpected("common+benchmark does not build a binary target");
    }

    std::string build_dir_base = profile_comp.getString("manifest.dirs.build").value_or("build");
    if (build_dir_base.empty())
        build_dir_base = "build";
    build_dir = catalyst::utils::yaml::multiplexedBuildDir(build_dir_base, profiles).string();

    if (auto provides = profile_comp.getString("manifest.provides"); provides && !provides->empty()) {
        exe = *provides;
    } else if (auto name = profile_comp.getString("manifest.name"); name && !name->empty()) {
        exe = *name;
    } else {
        return std::unexpected("Unable to determine executable name. "
                               "manifest.name and manifest.provides are undefined");
    }

    fs::path exe_path = fs::absolute(fs::path(std::format("{}/{}", build_dir, exe)));
    std::string command = commandStr(exe_path, args.params);

    catalyst::toolchain::ToolchainDef tc;
    if (auto tc_path = profile_comp.getString("manifest.toolchain")) {
        auto parsed = catalyst::toolchain::parseToolchain(*tc_path);
        if (!parsed) {
            return std::unexpected(std::format("Failed to load toolchain {}: {}", *tc_path, parsed.error()));
        }
        tc = std::move(*parsed);
    }

    Result<std::string> lib_path_res = catalyst::generate::libPath(profile_comp, profiles, tc);
    if (!lib_path_res) {
        return std::unexpected("Failed to generate LD_LIBRARY_PATH");
    }

    std::unordered_map<std::string, std::string> exec_env;
#if defined(_WIN32)
    exec_env["PATH"] = lib_path_res.value();
#elif defined(__APPLE__)
    exec_env["DYLD_LIBRARY_PATH"] = lib_path_res.value();
#else
    exec_env["LD_LIBRARY_PATH"] = lib_path_res.value();
#endif

    catalyst::logger.debug("Executing command: {}", command);

    std::vector<std::string> exec_args;
    exec_args.push_back(exe_path.string());
    exec_args.insert(exec_args.end(), args.params.begin(), args.params.end());

    if (!args.rebuild) {
        catalyst::logger.debug("Rebuild not requested, checking for staleness.");
        warnIfStale(exe_path, profile_comp, profiles);
    } else {
        catalyst::logger.debug("Rebuild requested, skipping staleness check.");
        if (auto build_res = catalyst::build::action({
                .regen = false,
                .force_rebuild = false,
                .force_refetch = false,
                .workspace_build = args.workspace.has_value(),
                .watch = false,
                .package = "",
                .profiles = profiles,
                .enabled_features = {},
                .backend = "",
                .workspace = std::nullopt, // Prevent build from recursing; bench handles workspace members
            });
            !build_res) {
            return build_res;
        }
    }

    if (int res = catalyst::processExec(std::move(exec_args), std::nullopt, exec_env).value().get(); res) {
        return std::unexpected(
            std::format("Target executable: {} exited with failure code: {}", exe_path.string(), res));
    }

    catalyst::logger.debug("Running post-bench hooks.");
    if (auto res = hooks::postBench(profile_comp); !res) {
        return res;
    }

    catalyst::logger.debug("Bench subcommand finished successfully.");
    return {};
}

} // namespace catalyst::bench
