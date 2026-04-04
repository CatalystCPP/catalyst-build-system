#include "catalyst/dispatch.hpp"
#include "catalyst/hooks.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::hooks {
std::expected<void, std::string> executeCatalystHook(const YAML::Node &item, const std::string &hook_name) {
    auto cat_node = item["catalyst"];
    if (cat_node.IsScalar()) {
        auto args = cat_node.as<std::string>();
        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, args);
        auto res = catalyst::dispatchHook("catalyst " + args);
        if (!res) {
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
        }
    } else if (cat_node.IsMap()) {
        if (!cat_node["subcommand"]) {
            return std::unexpected(std::format("Hook '{}' catalyst missing required 'subcommand' field", hook_name));
        }
        std::vector<std::string> args;
        if (cat_node["global_args"]) {
            for (const auto &a : cat_node["global_args"])
                args.push_back(a.as<std::string>());
        }
        args.push_back(cat_node["subcommand"].as<std::string>());
        if (cat_node["profiles"]) {
            for (const auto &p : cat_node["profiles"]) {
                args.emplace_back("-p");
                args.push_back(p.as<std::string>());
            }
        }
        if (cat_node["args"]) {
            for (const auto &a : cat_node["args"])
                args.push_back(a.as<std::string>());
        }

        std::string joined;
        constexpr auto JOINED_ARGS_RESERVE = 128;
        joined.reserve(JOINED_ARGS_RESERVE);
        for (const auto &a : args)
            joined += a + " ";
        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, joined);

        auto res = catalyst::dispatchHook(args);
        if (!res) {
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
        }
    } else {
        return std::unexpected(std::format("Hook '{}' catalyst has invalid type", hook_name));
    }
    return {};
}
} // namespace catalyst::hooks
