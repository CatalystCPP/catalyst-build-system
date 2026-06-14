#include <regex>
#include <span>
#include <string_view>

#include "catalyst/utils/result.hpp"
#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::hooks {

namespace {

Result<std::string> substituteCmdArgs(std::string cmd,
                                                          std::span<const std::string> inputs,
                                                          std::span<const std::string> outputs,
                                                          std::string_view hook_name);

// Collects a scalar-or-sequence node ("input"/"output") into a string list.
std::vector<std::string> collectPathList(ryml::ConstNodeRef node) {
    std::vector<std::string> paths;
    if (!node.readable())
        return paths;
    if (node.is_seq()) {
        for (ryml::ConstNodeRef item : node.children())
            if (auto s = catalyst::utils::yaml::asString(item))
                paths.push_back(*s);
    } else if (auto s = catalyst::utils::yaml::asString(node)) {
        paths.push_back(*s);
    }
    return paths;
}

} // namespace

Result<void> executeCodegenHook(ryml::ConstNodeRef codegen_node, std::string_view hook_name) {
    using utils::yaml::asString;
    using utils::yaml::child;

    auto cmd_opt = asString(child(codegen_node, "cmd"));
    if (!cmd_opt) {
        return std::unexpected(std::format("Hook '{}' codegen missing required 'cmd' field", hook_name));
    }
    std::string cmd = *cmd_opt;

    std::vector<std::string> inputs = collectPathList(child(codegen_node, "input"));
    std::vector<std::string> outputs = collectPathList(child(codegen_node, "output"));

    auto sub_res = substituteCmdArgs(cmd, inputs, outputs, hook_name);
    if (!sub_res) {
        return std::unexpected(sub_res.error());
    }
    cmd = sub_res.value();

    bool should_run = true;
    if (!inputs.empty() && !outputs.empty()) {
        should_run = false;
        std::filesystem::file_time_type oldest_out = std::filesystem::file_time_type::max();
        for (const auto &out : outputs) {
            if (!std::filesystem::exists(out)) {
                should_run = true;
                break;
            }
            auto mtime = std::filesystem::last_write_time(out);
            if (mtime < oldest_out)
                oldest_out = mtime;
        }

        if (!should_run) {
            for (const auto &in : inputs) {
                if (!std::filesystem::exists(in)) {
                    catalyst::logger.warn("[Catalyst Hook: {}] codegen input file not found: {}", hook_name, in);
                    should_run = true;
                    break;
                }
                auto mtime = std::filesystem::last_write_time(in);
                if (mtime > oldest_out) {
                    should_run = true;
                    break;
                }
            }
        }
    }

    if (should_run) {
        catalyst::logger.debug("[Catalyst Hook: {}] Running codegen: {}", hook_name, cmd);
        if (catalyst::processExec(shellCmd(cmd)).value().get() != 0) {
            return std::unexpected(std::format("Hook '{}' codegen failed: {}", hook_name, cmd));
        }
    } else {
        catalyst::logger.debug("[Catalyst Hook: {}] Skipping codegen: {} (up-to-date)", hook_name, cmd);
    }
    return {};
}

namespace {
Result<std::string> substituteCmdArgs(std::string cmd,
                                                          std::span<const std::string> inputs,
                                                          std::span<const std::string> outputs,
                                                          std::string_view hook_name) {
    // Substitute $IN/$OUT variables, indices, and slices in cmd.
    constexpr auto quote_if_needed = [](std::string_view path) -> std::string {
        if (path.empty())
            return std::string("''");
        if (path.find_first_of(" \t\n\r\"'\\$&|;<>()`~*?[]{}#^") == std::string_view::npos) {
            return std::string{path};
        }
        std::string escaped = "'";
        for (char c : path) {
            if (c == '\'')
                escaped += "'\\''";
            else
                escaped += c;
        }
        escaped += "'";
        return escaped;
    };

    std::string new_cmd;
    std::regex var_regex(R"((\$\$)|(\$(IN|OUT)(?:\[(?:(-?\d+)|(-?\d*)?:(-?\d*)?(?::(-?\d*)?)?)\])?))");
    std::sregex_iterator it(cmd.begin(), cmd.end(), var_regex);
    std::sregex_iterator end;

    constexpr auto parse_int = [](std::string_view s, ssize_t &out) -> bool {
        if (s.empty() || s == "-")
            return false;
        try {
            out = std::stoll(std::string{s});
            return true;
        } catch (...) {
            return false;
        }
    };

    size_t last_pos = 0;
    for (; it != end; ++it) {
        std::smatch match = *it;
        size_t match_pos = match.position();

        new_cmd.append(cmd.begin() + last_pos, cmd.begin() + match_pos);
        last_pos = match_pos + match.length();

        if (match[1].matched) {
            new_cmd.append("$");
            continue;
        }

        const std::string &var_name = match[3].str();
        std::span<const std::string> target_list = (var_name == "IN") ? inputs : outputs;
        ssize_t n = target_list.size();

        if (match[4].matched && match[4].length() > 0) {
            // Single index
            ssize_t idx;
            if (!parse_int(match[4].str(), idx)) {
                return std::unexpected(
                    std::format("Hook '{}' codegen cmd contains invalid index: '{}'", hook_name, match[4].str()));
            }
            if (idx < 0)
                idx += n;
            if (idx < 0 || idx >= n) {
                return std::unexpected(
                    std::format("Hook '{}' codegen cmd references out-of-bounds ${} index", hook_name, var_name));
            }
            new_cmd.append(quote_if_needed(target_list[idx]));
        } else {
            // Slice or bare variable
            ssize_t step = 1;
            if (match[7].matched && match[7].length() > 0) {
                if (!parse_int(match[7].str(), step)) {
                    return std::unexpected(
                        std::format("Hook '{}' codegen cmd has invalid slice step: '{}'", hook_name, match[7].str()));
                }
            }
            if (step == 0) {
                return std::unexpected(std::format("Hook '{}' codegen cmd has invalid slice step 0", hook_name));
            }

            ssize_t start;
            if (match[5].matched && match[5].length() > 0) {
                if (!parse_int(match[5].str(), start)) {
                    return std::unexpected(
                        std::format("Hook '{}' codegen cmd has invalid slice start: '{}'", hook_name, match[5].str()));
                }
                if (start < 0)
                    start += n;
                if (step > 0) {
                    start = std::max<ssize_t>(0, std::min<ssize_t>(n, start));
                } else {
                    start = std::max<ssize_t>(-1, std::min<ssize_t>(n - 1, start));
                }
            } else {
                start = (step > 0) ? 0 : n - 1;
            }

            ssize_t stop;
            if (match[6].matched && match[6].length() > 0) {
                if (!parse_int(match[6].str(), stop)) {
                    return std::unexpected(
                        std::format("Hook '{}' codegen cmd has invalid slice stop: '{}'", hook_name, match[6].str()));
                }
                if (stop < 0)
                    stop += n;
                if (step > 0) {
                    stop = std::max<ssize_t>(0, std::min<ssize_t>(n, stop));
                } else {
                    stop = std::max<ssize_t>(-1, std::min<ssize_t>(n - 1, stop));
                }
            } else {
                stop = (step > 0) ? n : -1;
            }

            // Cap step to prevent signed integer overflow
            if (step > n + 1)
                step = n + 1;
            if (step < -(n + 1))
                step = -(n + 1);

            bool first = true;
            if (step > 0) {
                for (ssize_t i = start; i < stop; i += step) {
                    if (!first)
                        new_cmd.append(" ");
                    new_cmd.append(quote_if_needed(target_list[i]));
                    first = false;
                }
            } else {
                for (ssize_t i = start; i > stop; i += step) {
                    if (!first)
                        new_cmd.append(" ");
                    new_cmd.append(quote_if_needed(target_list[i]));
                    first = false;
                }
            }
        }
    }
    new_cmd.append(cmd.begin() + last_pos, cmd.end());
    return new_cmd;
}
} // namespace
} // namespace catalyst::hooks
