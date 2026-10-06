#include <format>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/subcommands/build.hpp"

auto catalyst::build::parse(CLI::App &app) -> std::pair<CLI::App *, std::unique_ptr<Parse>> {
    CLI::App *build = app.add_subcommand("build", "Build the project.");
    auto ret = std::make_unique<Parse>();
    auto &opts = ret->cli_options;
    opts.emplace_back("regen", build->add_flag("-r,--regen", ret->regen, "Regenerate the build file.")->default_val(false));
    opts.emplace_back("force_rebuild",
                      build->add_flag("-b,--force-rebuild", ret->force_rebuild, "Recompile dependencies.")
                          ->default_val(false));
    opts.emplace_back(
        "force_refetch",
        build->add_flag("--force-refetch", ret->force_refetch, "Refetch dependencies.")->default_val(false));
    opts.emplace_back("workspace_build",
                      build->add_flag("--workspace,--all", ret->workspace_build, "Build all members in the workspace.")
                          ->default_val(false));
    opts.emplace_back(
        "watch",
        build->add_flag("--watch,-w", ret->watch, "Continuous build mode. Rebuilds on source file changes.")
            ->default_val(false));
    opts.emplace_back("explain",
                      build
                          ->add_flag("--explain",
                                     ret->explain,
                                     "Explain build decisions on stderr and in catalyst_explain_<UTC datetime>.md. "
                                     "Incompatible with --watch.")
                          ->default_val(false));
    opts.emplace_back("package",
                      build->add_option("-P,--package", ret->package, "Build a specific package from the root."));
    opts.emplace_back("profiles",
                      build->add_option("-p,--profiles", ret->profiles, "Profile composition to build.")
                          ->default_val(std::vector{"common"}));
    opts.emplace_back("enabled_features",
                      build->add_option("-f,--features", ret->enabled_features, "Features to enable.")
                          ->default_val(std::vector<std::string>{}));
    opts.emplace_back(
        "backend",
        build->add_option("--backend", ret->backend, "Backend to use for generation (ninja, gmake, cob)."));
    return {build, std::move(ret)};
}

std::string catalyst::build::describe(const Parse &args) {
    auto origin = [&](std::string_view field) -> std::string_view {
        for (const auto &[name, option] : args.cli_options)
            if (name == field)
                return option != nullptr && option->count() > 0 ? "command line" : "default";
        return "internal";
    };
    auto list = [](const std::vector<std::string> &values) {
        std::string out = "[";
        for (const auto &value : values)
            out += (out.size() == 1 ? "" : ", ") + value;
        return out + "]";
    };
    std::string out = "catalyst::build::Parse\n";
    out += std::format("regen: {} ({})\n", args.regen, origin("regen"));
    out += std::format("force_rebuild: {} ({})\n", args.force_rebuild, origin("force_rebuild"));
    out += std::format("force_refetch: {} ({})\n", args.force_refetch, origin("force_refetch"));
    out += std::format("workspace_build: {} ({})\n", args.workspace_build, origin("workspace_build"));
    out += std::format("watch: {} ({})\n", args.watch, origin("watch"));
    out += std::format("explain: {} ({})\n", args.explain, origin("explain"));
    out += std::format("package: '{}' ({})\n", args.package, origin("package"));
    out += std::format("profiles: {} ({})\n", list(args.profiles), origin("profiles"));
    out += std::format("enabled_features: {} ({})\n", list(args.enabled_features), origin("enabled_features"));
    out += std::format("backend: '{}' ({})\n", args.backend, origin("backend"));
    out += std::format("workspace: {}\n",
                       args.workspace ? std::filesystem::absolute(args.workspace->getRoot()).string()
                                      : std::string{"<none>"});
    out += std::format("executable_path: {}", args.executable_path.string());
    return out;
}
