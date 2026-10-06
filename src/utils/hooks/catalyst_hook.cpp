#include <ranges>
#include <string>
#include <string_view>

#include "catalyst/dispatch.hpp"
#include "catalyst/hooks.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace {
thread_local int s_catalyst_hook_depth = 0;
constexpr int s_max_catalyst_hook_depth = 16;
} // namespace

auto catalyst::hooks::executeCatalystHook(ryml::ConstNodeRef item,
                                          std::string_view hook_name,
                                          [[maybe_unused]] const HookEnvironment &environment) -> Result<void> {
    using utils::yaml::asString;
    using utils::yaml::child;

    if (s_catalyst_hook_depth >= s_max_catalyst_hook_depth) {
        catalyst::logger.explain("Internal Catalyst hook recursion limit ({}) exceeded for '{}'",
                                 s_max_catalyst_hook_depth, hook_name);
        return std::unexpected(std::format("Internal Catalyst hook recursion limit ({}) exceeded", s_max_catalyst_hook_depth));
    }
    struct DepthGuard {
        ~DepthGuard() { --s_catalyst_hook_depth; }
    } guard;
    ++s_catalyst_hook_depth;

    ryml::ConstNodeRef cat_node = child(item, "catalyst");
    if (auto scalar_args = asString(cat_node)) {
        catalyst::logger.explain("Executing internal Catalyst hook for '{}' (depth: {}/{}): 'catalyst {}'",
                                 hook_name, s_catalyst_hook_depth, s_max_catalyst_hook_depth, *scalar_args);
        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, *scalar_args);
        if (auto res = catalyst::dispatchHook("catalyst " + *scalar_args); !res) {
            catalyst::logger.explain("Internal Catalyst hook for '{}' failed: {}", hook_name, res.error());
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
        }
        catalyst::logger.explain("Internal Catalyst hook for '{}' completed successfully (depth: {})", hook_name,
                                 s_catalyst_hook_depth);
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

        catalyst::logger.explain("Executing internal Catalyst hook for '{}' (depth: {}/{}): 'catalyst {}'",
                                 hook_name, s_catalyst_hook_depth, s_max_catalyst_hook_depth, joined);
        catalyst::logger.debug("[Catalyst Hook: {}] Running catalyst: {}", hook_name, joined);

        auto res = catalyst::dispatchHook(args);
        if (!res) {
            catalyst::logger.explain("Internal Catalyst hook for '{}' failed: {}", hook_name, res.error());
            return std::unexpected(std::format("Hook '{}' catalyst dispatch failed: {}", hook_name, res.error()));
        }
        catalyst::logger.explain("Internal Catalyst hook for '{}' completed successfully (depth: {})", hook_name,
                                 s_catalyst_hook_depth);
    } else
        return std::unexpected(std::format("Hook '{}' catalyst has invalid type", hook_name));

    return {};
}
