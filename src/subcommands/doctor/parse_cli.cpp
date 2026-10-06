#include "catalyst/subcommands/doctor.hpp"

namespace catalyst::doctor {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    auto *subcommand = app.add_subcommand("doctor", "Diagnose project readiness without building or running hooks.");
    auto result = std::make_unique<Parse>();
    subcommand->add_option("-p,--profiles", result->profiles, "Profile composition to inspect.")
        ->default_val(std::vector<std::string>{"common"});
    subcommand->add_option("-f,--features", result->enabled_features, "Feature overrides to validate.");
    subcommand->add_option("--backend", result->backend, "Override the configured build backend.");
    subcommand->add_flag("--json", result->json, "Print a machine-readable diagnostic report.");
    subcommand->add_flag("--strict", result->strict, "Fail on warnings as well as errors.");
    return {subcommand, std::move(result)};
}
} // namespace catalyst::doctor
