#include <ranges>
#include <string>
#include <string_view>

#include "catalyst/dispatch.hpp"
#include "catalyst/hooks.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

auto catalyst::hooks::executeCatalystHook(ryml::ConstNodeRef item, std::string_view hook_name) -> Result<void> {
    using utils::yaml::asString;
    using utils::yaml::child;

    ryml::ConstNodeRef cat_node = child(item, "catalyst");
    if (auto scalar_args = asString(cat_node)) {
        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, *scalar_args);
        if (auto res = catalyst::dispatchHook("catalyst " + *scalar_args); !res)
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
    } else if (cat_node.readable() && cat_node.is_map()) {

        auto subcommand = asString(child(cat_node, "subcommand"));
        if (!subcommand)
            return std::unexpected(std::format("Hook '{}' catalyst missing required 'subcommand' field", hook_name));
        std::vector<std::string> args;
        auto append_strings = [&args](ryml::ConstNodeRef seq) constexpr -> void {
            if (!seq.readable() || !seq.is_seq())
                return;
            for (ryml::ConstNodeRef a : seq.children())
                if (auto s = utils::yaml::asString(a))
                    args.push_back(*s);
        };
        append_strings(child(cat_node, "global_args"));
        args.push_back(*subcommand);
        if (ryml::ConstNodeRef profiles = child(cat_node, "profiles"); profiles.readable() && profiles.is_seq())
            for (ryml::ConstNodeRef p : profiles.children()) {
                if (auto s = asString(p)) {
                    args.emplace_back("-p");
                    args.push_back(*s);
                }
            }
        append_strings(child(cat_node, "args"));

        std::string joined{std::from_range, args | std::views::join_with(' ')};

        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, joined);

        auto res = catalyst::dispatchHook(args);
        if (!res)
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
    } else
        return std::unexpected(std::format("Hook '{}' catalyst has invalid type", hook_name));

    return {};
}
