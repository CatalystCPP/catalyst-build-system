#include <algorithm>
#include <filesystem>
#include <print>
#include <string>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/subcommands/completion.hpp"

namespace {

std::string sanitizeState(const std::string &name) {
    std::string s = name;
    std::ranges::replace(s, '-', '_');
    return s;
}

void printTransitions(const CLI::App *app, const std::string &state_path) {
    auto subcommands = app->get_subcommands([](const CLI::App *a) { return !a->get_name().empty(); });
    std::vector<const CLI::App *> valid_subs;
    for (const auto *sub : subcommands) {
        if (sub->get_name() != "help" && !sub->get_name().empty()) {
            valid_subs.push_back(sub);
        }
    }
    if (!valid_subs.empty()) {
        std::println("            \"{}\")", state_path);
        std::println("                case \"${{word}}\" in");
        for (const auto *sub : valid_subs) {
            std::string sub_name = sub->get_name();
            std::println("                    {}) cmd=\"{}_{}\" ;;", sub_name, state_path, sanitizeState(sub_name));
        }
        std::println("                esac");
        std::println("                ;;");
    }
    for (const auto *sub : valid_subs) {
        std::string sub_name = sub->get_name();
        printTransitions(sub, state_path + "_" + sanitizeState(sub_name));
    }
}

void printCompletions(const CLI::App *app, const std::string &state_path) {
    auto subcommands = app->get_subcommands([](const CLI::App *a) { return !a->get_name().empty(); });
    std::vector<std::string> subnames;
    for (const auto *sub : subcommands) {
        if (!sub->get_name().empty()) {
            subnames.push_back(sub->get_name());
        }
    }

    std::vector<std::string> flags;
    for (const auto *opt : app->get_options()) {
        if (opt->get_positional())
            continue;
        for (const auto &lname : opt->get_lnames()) {
            flags.push_back("--" + lname);
        }
        for (const auto &sname : opt->get_snames()) {
            flags.push_back("-" + sname);
        }
    }

    std::println("        \"{}\")", state_path);
    if (!flags.empty() && !subnames.empty()) {
        std::println("            if [[ \"$cur\" == -* ]]; then");
        std::print("                COMPREPLY=( $(compgen -W \"");
        for (size_t i = 0; i < flags.size(); ++i) {
            std::print("{}{}", flags[i], (i + 1 == flags.size() ? "" : " "));
        }
        std::println("\" -- \"$cur\") )");
        std::println("            else");
        std::print("                COMPREPLY=( $(compgen -W \"");
        for (size_t i = 0; i < subnames.size(); ++i) {
            std::print("{}{}", subnames[i], (i + 1 == subnames.size() ? "" : " "));
        }
        std::println("\" -- \"$cur\") )");
        std::println("            fi");
    } else if (!flags.empty()) {
        std::print("            COMPREPLY=( $(compgen -W \"");
        for (size_t i = 0; i < flags.size(); ++i) {
            std::print("{}{}", flags[i], (i + 1 == flags.size() ? "" : " "));
        }
        std::println("\" -- \"$cur\") )");
    } else if (!subnames.empty()) {
        std::print("            COMPREPLY=( $(compgen -W \"");
        for (size_t i = 0; i < subnames.size(); ++i) {
            std::print("{}{}", subnames[i], (i + 1 == subnames.size() ? "" : " "));
        }
        std::println("\" -- \"$cur\") )");
    } else {
        std::println("            COMPREPLY=( $(compgen -f -- \"$cur\") )");
    }
    std::println("            ;;");

    for (const auto *sub : subcommands) {
        if (sub->get_name() == "help" || sub->get_name().empty())
            continue;
        std::string sub_name = sub->get_name();
        printCompletions(sub, state_path + "_" + sanitizeState(sub_name));
    }
}

void printBashCompletionScript(const CLI::App &app) {
    std::string name = app.get_name();
    if (!name.empty()) {
        name = std::filesystem::path(name).filename().string();
    }
    if (name.empty()) {
        name = "catalyst";
    }

    std::println("_{}() {{", name);
    std::println("    local cur prev words cword");
    std::println("    if declare -F _init_completion >/dev/null 2>&1; then");
    std::println("        _init_completion || return");
    std::println("    else");
    std::println("        COMPREPLY=()");
    std::println("        _get_comp_words_by_ref -n =: cur prev words cword");
    std::println("    fi\n");
    std::println("    local cmd=\"{}\"", name);
    std::println("    local i=1");
    std::println("    while [ $i -lt $cword ]; do");
    std::println("        local word=\"${{words[i]}}\"");
    std::println("        case \"${{cmd}}\" in");

    printTransitions(&app, name);

    std::println("        esac");
    std::println("        i=$((i+1))");
    std::println("    done\n");
    std::println("    case \"${{cmd}}\" in");

    printCompletions(&app, name);

    std::println("    esac");
    std::println("}}");
    std::println("\ncomplete -F _{0} {0}", name);
}

void printZshCompletionScript(const CLI::App &app) {
    std::string name = app.get_name();
    if (!name.empty()) {
        name = std::filesystem::path(name).filename().string();
    }
    if (name.empty()) {
        name = "catalyst";
    }

    std::println("#compdef {}", name);
    std::println("if ! type _init_completion &>/dev/null; then");
    std::println("    autoload -U +X compinit && compinit");
    std::println("    autoload -U +X bashcompinit && bashcompinit");
    std::println("fi\n");
    printBashCompletionScript(app);
}

} // namespace

auto catalyst::completion::action(const Parse &parse_args, const CLI::App &app) -> Result<void> {
    if (parse_args.shell == "bash") {
        printBashCompletionScript(app);
    } else if (parse_args.shell == "zsh") {
        printZshCompletionScript(app);
    } else {
        return std::unexpected(std::format("Unsupported shell: {}", parse_args.shell));
    }
    return {};
}
