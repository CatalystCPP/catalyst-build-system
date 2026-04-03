#include <regex>

#include "catalyst/hooks.hpp"
#include "catalyst/process_exec.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::hooks {
std::expected<void, std::string> executeCodegenHook(const YAML::Node &codegen_node, const std::string &hook_name) {
    if (!codegen_node["cmd"]) {
        return std::unexpected(std::format("Hook '{}' codegen missing required 'cmd' field", hook_name));
    }
    auto cmd = codegen_node["cmd"].as<std::string>();

    std::vector<std::string> inputs;
    if (codegen_node["input"]) {
        if (codegen_node["input"].IsSequence()) {
            for (const auto &in : codegen_node["input"])
                inputs.push_back(in.as<std::string>());
        } else if (codegen_node["input"].IsScalar()) {
            inputs.push_back(codegen_node["input"].as<std::string>());
        }
    }

    std::vector<std::string> outputs;
    if (codegen_node["output"]) {
        if (codegen_node["output"].IsSequence()) {
            for (const auto &out : codegen_node["output"])
                outputs.push_back(out.as<std::string>());
        } else if (codegen_node["output"].IsScalar()) {
            outputs.push_back(codegen_node["output"].as<std::string>());
        }
    }

    // Substitute $IN/$OUT variables, indices, and slices in cmd.
    auto quote_if_needed = [](const std::string &path) {
        if (path.find(' ') != std::string::npos) {
            return "\"" + path + "\"";
        }
        return path;
    };

    std::string new_cmd;
    std::regex var_regex(R"(\$(IN|OUT)(?:\[(?:(-?\d+)|(-?\d*)?:(-?\d*)?(?::(-?\d*)?)?)\])?)");
    std::sregex_iterator it(cmd.begin(), cmd.end(), var_regex);
    std::sregex_iterator end;

    size_t last_pos = 0;
    for (; it != end; ++it) {
        std::smatch match = *it;
        size_t match_pos = match.position();

        // Handle $$ escape
        if (match_pos > 0 && cmd[match_pos - 1] == '$') {
            new_cmd.append(cmd.begin() + last_pos, cmd.begin() + match_pos - 1);
            new_cmd.append(match.str());
            last_pos = match_pos + match.length();
            continue;
        }

        new_cmd.append(cmd.begin() + last_pos, cmd.begin() + match_pos);
        last_pos = match_pos + match.length();

        const std::vector<std::string> &target_list = (match[1].str() == "IN") ? inputs : outputs;
        ssize_t n = target_list.size();

        if (match[2].matched && match[2].length() > 0) {
            // Single index
            ssize_t idx = std::stoll(match[2].str());
            if (idx < 0)
                idx += n;
            if (idx < 0 || idx >= n) {
                return std::unexpected(
                    std::format("Hook '{}' codegen cmd references out-of-bounds ${} index", hook_name, match[1].str()));
            }
            new_cmd.append(quote_if_needed(target_list[idx]));
        } else {
            // Slice or bare variable
            ssize_t step = 1;
            if (match[5].matched && match[5].length() > 0) {
                step = std::stoll(match[5].str());
            }
            if (step == 0) {
                return std::unexpected(std::format("Hook '{}' codegen cmd has invalid slice step 0", hook_name));
            }

            ssize_t start;
            if (match[3].matched && match[3].length() > 0) {
                start = std::stoll(match[3].str());
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
            if (match[4].matched && match[4].length() > 0) {
                stop = std::stoll(match[4].str());
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
    cmd = new_cmd;

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
} // namespace catalyst::hooks
