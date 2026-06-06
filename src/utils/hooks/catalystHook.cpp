#include <ranges>
#include <string>

#include "catalyst/dispatch.hpp"
#include "catalyst/hooks.hpp"
#include "catalyst/utils/log/log.hpp"

auto catalyst::hooks::executeCatalystHook(const YAML::Node &item, const std::string &hook_name)
    -> std::expected<void, std::string> {
    if (auto cat_node = item["catalyst"]; cat_node.IsScalar()) {
        auto args = cat_node.as<std::string>();
        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, args);
        if (auto res = catalyst::dispatchHook("catalyst " + args); !res)
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
    } else if (cat_node.IsMap()) {

        if (!cat_node["subcommand"])
            return std::unexpected(std::format("Hook '{}' catalyst missing required 'subcommand' field", hook_name));
        std::vector<std::string> args;
        if (cat_node["global_args"])
            for (const auto &a : cat_node["global_args"])
                args.push_back(a.as<std::string>());
        args.push_back(cat_node["subcommand"].as<std::string>());
        if (cat_node["profiles"])
            for (const auto &p : cat_node["profiles"]) {
                args.emplace_back("-p");
                args.emplace_back(p.as<std::string>());
            }
        if (cat_node["args"])
            for (const auto &a : cat_node["args"])
                args.push_back(a.as<std::string>());

        std::string joined{std::from_range, args | std::views::join_with(' ')};

        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, joined);

        auto res = catalyst::dispatchHook(args);
        if (!res)
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
    } else
        return std::unexpected(std::format("Hook '{}' catalyst has invalid type", hook_name));

    return {};
}
