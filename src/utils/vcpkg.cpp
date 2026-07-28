#include "catalyst/utils/vcpkg.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "catalyst/process_exec.hpp"
#include "catalyst/utils/result.hpp"

namespace catalyst::utils::vcpkg {
namespace fs = std::filesystem;

namespace {

std::string trim(std::string value) {
    const auto first = std::ranges::find_if_not(value, [](unsigned char c) { return std::isspace(c) != 0; });
    value.erase(value.begin(), first);
    const auto last =
        std::ranges::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; });
    value.erase(last.base(), value.end());
    return value;
}

std::string jsonString(std::string_view value) {
    std::string result{"\""};
    for (const char c : value) {
        switch (c) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += c;
                break;
        }
    }
    result += '"';
    return result;
}

Result<void> validateDependencies(std::span<const Dependency> dependencies) {
    std::unordered_map<std::string, const Dependency *> seen;
    for (const Dependency &dependency : dependencies) {
        if (dependency.name.empty()) {
            return std::unexpected("A vcpkg dependency is missing its name.");
        }
        if (dependency.triplet.empty()) {
            return std::unexpected(std::format("vcpkg dependency '{}' is missing its triplet.", dependency.name));
        }
        const std::string dependency_key = dependency.name + '\x1f' + dependency.triplet;
        if (const auto existing = seen.find(dependency_key); existing != seen.end()) {
            const Dependency &previous = *existing->second;
            if (previous.version != dependency.version || previous.port_version != dependency.port_version
                || previous.features != dependency.features) {
                return std::unexpected(std::format(
                    "Conflicting vcpkg declarations for '{}' in triplet '{}'.", dependency.name, dependency.triplet));
            }
        } else {
            seen.emplace(dependency_key, &dependency);
        }
    }
    return {};
}

Result<void> writeManifest(const fs::path &manifest_dir,
                           std::span<const Dependency> dependencies,
                           std::string_view builtin_baseline) {
    auto json = manifestJson(dependencies, builtin_baseline);
    if (!json) {
        return std::unexpected(json.error());
    }

    std::error_code ec;
    fs::create_directories(manifest_dir, ec);
    if (ec) {
        return std::unexpected(
            std::format("Failed to create vcpkg manifest directory '{}': {}", manifest_dir.string(), ec.message()));
    }

    std::ofstream output(manifest_dir / "vcpkg.json");
    if (!output) {
        return std::unexpected(std::format("Failed to write vcpkg manifest in '{}'.", manifest_dir.string()));
    }
    output << *json;
    return {};
}

std::optional<std::string> fieldValue(std::string_view line, std::string_view field) {
    if (!line.starts_with(field) || line.size() <= field.size() || line[field.size()] != ':') {
        return std::nullopt;
    }
    return trim(std::string(line.substr(field.size() + 1)));
}

} // namespace

std::string statusKey(std::string_view name, std::string_view triplet) {
    std::string result{name};
    result.push_back('\x1f');
    result += triplet;
    return result;
}

Result<std::string> currentBuiltinBaseline(const fs::path &vcpkg_root) {
    auto result = catalyst::processExecStdout({"git", "-C", vcpkg_root.string(), "rev-parse", "HEAD"});
    if (!result) {
        return std::unexpected(std::format("Failed to determine the vcpkg builtin baseline: {}", result.error()));
    }
    std::string baseline = trim(*result);
    if (baseline.empty()) {
        return std::unexpected("vcpkg returned an empty builtin baseline.");
    }
    return baseline;
}

Result<std::string> manifestJson(std::span<const Dependency> dependencies, std::string_view builtin_baseline) {
    if (builtin_baseline.empty()) {
        return std::unexpected("A vcpkg builtin baseline is required for manifest mode.");
    }
    if (auto valid = validateDependencies(dependencies); !valid) {
        return std::unexpected(valid.error());
    }

    std::ostringstream output;
    output << "{\n"
           << "  \"name\": \"catalyst-dependencies\",\n"
           << "  \"version-string\": \"0\",\n"
           << "  \"builtin-baseline\": " << jsonString(builtin_baseline) << ",\n"
           << "  \"dependencies\": [";

    for (std::size_t index = 0; index < dependencies.size(); ++index) {
        const Dependency &dependency = dependencies[index];
        output << (index == 0 ? "\n" : ",\n") << "    ";
        if (dependency.features.empty()) {
            output << jsonString(dependency.name);
        } else {
            output << "{\"name\": " << jsonString(dependency.name) << ", \"features\": [";
            for (std::size_t feature_index = 0; feature_index < dependency.features.size(); ++feature_index) {
                if (feature_index != 0) {
                    output << ", ";
                }
                output << jsonString(dependency.features[feature_index]);
            }
            output << "]}";
        }
    }
    if (!dependencies.empty()) {
        output << '\n';
    }
    output << "  ]";

    std::vector<const Dependency *> overrides;
    for (const Dependency &dependency : dependencies) {
        if (!dependency.version.empty() && dependency.version != "latest") {
            overrides.push_back(&dependency);
        }
    }
    if (!overrides.empty()) {
        output << ",\n  \"overrides\": [\n";
        for (std::size_t index = 0; index < overrides.size(); ++index) {
            const Dependency &dependency = *overrides[index];
            output << "    {\"name\": " << jsonString(dependency.name)
                   << ", \"version\": " << jsonString(dependency.version);
            if (dependency.port_version.has_value()) {
                output << ", \"port-version\": " << *dependency.port_version;
            }
            output << '}' << (index + 1 == overrides.size() ? "\n" : ",\n");
        }
        output << "  ]";
    }
    output << "\n}\n";
    return output.str();
}

std::unordered_map<std::string, InstalledPackage> parseInstalledVersions(std::string_view status) {
    std::unordered_map<std::string, InstalledPackage> result;
    std::string package;
    std::string triplet;
    std::string version;
    std::string state;
    int port_version = 0;

    auto flush = [&]() {
        const bool installed =
            state.find("installed") != std::string::npos && state.find("not-installed") == std::string::npos;
        if (installed && !package.empty() && !triplet.empty() && !version.empty()) {
            result[statusKey(package, triplet)] = InstalledPackage{.version = version, .port_version = port_version};
        }
        package.clear();
        triplet.clear();
        version.clear();
        state.clear();
        port_version = 0;
    };

    std::size_t position = 0;
    while (position <= status.size()) {
        const std::size_t newline = status.find('\n', position);
        const std::size_t end = newline == std::string_view::npos ? status.size() : newline;
        const std::string_view line = status.substr(position, end - position);
        if (trim(std::string(line)).empty()) {
            flush();
        } else if (auto value = fieldValue(line, "Package")) {
            package = *value;
        } else if (auto value = fieldValue(line, "Architecture")) {
            triplet = *value;
        } else if (auto value = fieldValue(line, "Version")) {
            version = *value;
        } else if (auto value = fieldValue(line, "Port-Version")) {
            try {
                port_version = std::stoi(*value);
            } catch (...) {
                port_version = 0;
            }
        } else if (auto value = fieldValue(line, "Status")) {
            state = *value;
        }

        if (newline == std::string_view::npos) {
            break;
        }
        position = newline + 1;
    }
    flush();
    return result;
}

Result<InstallResult> install(std::span<const Dependency> dependencies,
                              const fs::path &build_dir,
                              const std::optional<std::string> &builtin_baseline) {
    if (dependencies.empty()) {
        return InstallResult{};
    }
    if (auto valid = validateDependencies(dependencies); !valid) {
        return std::unexpected(valid.error());
    }

    const char *vcpkg_root_env = std::getenv("VCPKG_ROOT");
    if (vcpkg_root_env == nullptr) {
        return std::unexpected(
            "VCPKG_ROOT environment variable not set. Please set it to your vcpkg installation directory.");
    }
    const fs::path vcpkg_root{vcpkg_root_env};
    fs::path vcpkg_executable = vcpkg_root / "vcpkg";
#if defined(_WIN32)
    vcpkg_executable.replace_extension(".exe");
#endif

    std::string baseline;
    if (builtin_baseline && !builtin_baseline->empty()) {
        baseline = *builtin_baseline;
    } else {
        auto resolved_baseline = currentBuiltinBaseline(vcpkg_root);
        if (!resolved_baseline) {
            return std::unexpected(resolved_baseline.error());
        }
        baseline = *resolved_baseline;
    }

    std::map<std::string, std::vector<Dependency>> by_triplet;
    for (const Dependency &dependency : dependencies) {
        by_triplet[dependency.triplet].push_back(dependency);
    }

    const fs::path install_root = build_dir / "vcpkg_installed";
    for (const auto &[triplet, triplet_dependencies] : by_triplet) {
        const fs::path manifest_dir = build_dir / "vcpkg-manifests" / triplet;
        if (auto written = writeManifest(manifest_dir, triplet_dependencies, baseline); !written) {
            return std::unexpected(written.error());
        }

        std::vector<std::string> args = {vcpkg_executable.string(),
                                         "install",
                                         "--x-manifest-root=" + manifest_dir.string(),
                                         "--x-install-root=" + install_root.string(),
                                         "--triplet=" + triplet};
        auto process = catalyst::processExec(std::move(args));
        if (!process) {
            return std::unexpected(std::format("Failed to start vcpkg manifest installation: {}", process.error()));
        }
        if (process->get() != 0) {
            return std::unexpected(std::format("vcpkg manifest installation failed for triplet '{}'.", triplet));
        }
    }

    const fs::path status_path = install_root / "vcpkg" / "status";
    std::ifstream status_file(status_path);
    if (!status_file) {
        return std::unexpected(
            std::format("vcpkg did not produce an installed status file at '{}'.", status_path.string()));
    }
    std::stringstream status;
    status << status_file.rdbuf();
    return InstallResult{
        .builtin_baseline = baseline, .install_root = install_root, .packages = parseInstalledVersions(status.str())};
}

} // namespace catalyst::utils::vcpkg
