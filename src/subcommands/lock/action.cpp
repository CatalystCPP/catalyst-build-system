#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/lock.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::lock {
namespace fs = std::filesystem;

namespace {

Result<std::string> resolveGitHash(const std::string &url, const std::string &version) {
    catalyst::logger.debug("Resolving git hash for {}@{}", url, version);

    std::string ref = version;
    if (version == "latest") {
        ref = "HEAD";
    }

    auto res = catalyst::processExecStdout({"git", "ls-remote", url, ref});
    if (!res) {
        return std::unexpected(std::format("Failed to resolve git hash for {}: {}", url, res.error()));
    }

    std::string output = res.value();
    if (output.empty()) {
        // Try as a tag/branch ref if HEAD didn't work or if it's a specific version
        res = catalyst::processExecStdout({"git", "ls-remote", url, "refs/tags/" + ref});
        if (res && !res.value().empty())
            output = res.value();
        else {
            res = catalyst::processExecStdout({"git", "ls-remote", url, "refs/heads/" + ref});
            if (res && !res.value().empty())
                output = res.value();
        }
    }

    if (output.empty()) {
        return std::unexpected(std::format("No remote ref found for {} at {}", url, version));
    }

    // Output is usually "<hash>\t<ref>\n"
    size_t tab_pos = output.find('\t');
    if (tab_pos == std::string::npos) {
        size_t space_pos = output.find(' ');
        if (space_pos == std::string::npos) {
            constexpr int BEST_EFFORT_LEN = 40;
            return output.substr(0, BEST_EFFORT_LEN); // Best effort
        }
        return output.substr(0, space_pos);
    }
    return output.substr(0, tab_pos);
}

struct DependencyInfo {
    std::string name;
    std::string source;
    std::string url;
    std::string version;
    std::string hash;
    std::string triplet;
    std::string path;

    bool operator==(const DependencyInfo &other) const {
        return name == other.name;
    }
};

void collectDependencies(const utils::yaml::Configuration &config,
                         std::unordered_map<std::string, DependencyInfo> &locked_deps,
                         const std::optional<Workspace> &workspace) {
    namespace yaml = utils::yaml;
    ryml::ConstNodeRef deps = yaml::child(config.rootRef(), "dependencies");
    if (!deps.readable() || !deps.is_seq())
        return;
    for (ryml::ConstNodeRef dep : deps.children()) {
        auto name_opt = yaml::asString(yaml::child(dep, "name"));
        auto source_opt = yaml::asString(yaml::child(dep, "source"));
        if (!name_opt || !source_opt)
            continue;

        const std::string &name = *name_opt;

        // Skip if it's a workspace member
        if (workspace && workspace->findPackage(name)) {
            catalyst::logger.debug("Skipping workspace dependency: {}", name);
            continue;
        }

        DependencyInfo info;
        info.name = name;
        info.source = *source_opt;

        if (info.source == "git") {
            info.url = yaml::asString(yaml::child(dep, "url")).value_or(info.source);
            info.version = yaml::asString(yaml::child(dep, "version")).value_or("latest");
        } else if (info.source == "vcpkg") {
            info.version = yaml::asString(yaml::child(dep, "version")).value_or("");
            info.triplet = yaml::asString(yaml::child(dep, "triplet")).value_or("");
        } else if (info.source == "local") {
            info.path = yaml::asString(yaml::child(dep, "path")).value_or("");
        }

        locked_deps[name] = info;
    }
}

} // namespace

Result<void> action(const Parse &parse_args) {
    catalyst::logger.debug("Lock subcommand invoked.");

    std::unordered_map<std::string, DependencyInfo> locked_deps;
    fs::path lockfile_path = "catalyst.lock";

    if (parse_args.workspace) {
        catalyst::logger.info("Resolving dependencies for workspace...");
        lockfile_path = parse_args.workspace->getRoot() / "catalyst.lock";

        // Load configurations in parallel, then collect dependencies sequentially
        // for deterministic resolution order
        const auto &members = parse_args.workspace->getMembers();
        struct MemberConfig {
            std::string name;
            std::optional<utils::yaml::Configuration> config;
        };
        std::vector<MemberConfig> configs(members.size());
        std::vector<std::thread> threads;

        size_t idx = 0;
        for (const auto &[name, member] : members) {
            configs[idx].name = name;
            threads.emplace_back([&cfg = configs[idx], m = member]() {
                std::vector<std::string> profiles = m.profiles;
                if (profiles.empty())
                    profiles = {"common"};

                try {
                    cfg.config.emplace(profiles, m.path);
                } catch (const std::exception &e) {
                    catalyst::logger.warn("Failed to load configuration for member {}: {}", cfg.name, e.what());
                }
            });
            ++idx;
        }
        for (auto &t : threads) {
            t.join();
        }

        for (const auto &mc : configs) {
            if (mc.config)
                collectDependencies(*mc.config, locked_deps, parse_args.workspace);
        }
    } else {
        utils::yaml::Configuration config(parse_args.profiles);
        collectDependencies(config, locked_deps, std::nullopt);
    }

    std::println(std::cout, "Locking {} dependencies...", locked_deps.size());

    namespace yaml = utils::yaml;
    ryml::Tree lock_tree;
    ryml::NodeRef lock_root = lock_tree.rootref();
    lock_root |= ryml::MAP;
    lock_root["lockfile_version"] = "1.0.0";
    ryml::NodeRef deps_node = yaml::childOrCreate(lock_root, "dependencies");
    deps_node |= ryml::SEQ;

    for (auto &[name, info] : locked_deps) {
        if (info.source == "git") {
            std::println(std::cout, "Resolving {}...", name);
            auto hash_res = resolveGitHash(info.url, info.version);
            if (!hash_res) {
                return std::unexpected(hash_res.error());
            }
            info.hash = hash_res.value();
            catalyst::logger.info("Resolved {} to {}", name, info.hash);
        }

        ryml::NodeRef dep_node = deps_node.append_child();
        dep_node |= ryml::MAP;
        dep_node["name"] = lock_tree.to_arena(info.name);
        dep_node["source"] = lock_tree.to_arena(info.source);
        if (!info.url.empty())
            dep_node["url"] = lock_tree.to_arena(info.url);
        if (!info.version.empty())
            dep_node["version"] = lock_tree.to_arena(info.version);
        if (!info.hash.empty())
            dep_node["hash"] = lock_tree.to_arena(info.hash);
        if (!info.triplet.empty())
            dep_node["triplet"] = lock_tree.to_arena(info.triplet);
        if (!info.path.empty())
            dep_node["path"] = lock_tree.to_arena(info.path);
    }

    std::ofstream fout(lockfile_path);
    if (!fout.is_open()) {
        return std::unexpected(std::format("Failed to open {} for writing.", lockfile_path.string()));
    }
    fout << yaml::emitYaml(lock_tree);

    std::println(std::cout, "Generated lockfile at {}", lockfile_path.string());
    return {};
}

} // namespace catalyst::lock
