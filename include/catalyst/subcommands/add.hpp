#pragma once
#include <expected>
#include <string>
#include <vector>

#include <CLI11.hpp>

#include "catalyst/utils/result.hpp"

namespace catalyst::add {
struct Parse {
    std::string name;
    std::string version{"latest"};
    std::string source{"catalyst_hub"};
    std::vector<std::string> profiles{{"common"}};
    std::vector<std::string> enabled_features;
};

namespace conan {
struct Parse {
    std::string name;
    std::string version;
    std::vector<std::string> profiles{{"common"}};
};
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace conan

namespace git {
struct Parse {
    std::string name;
    std::string remote;
    std::string version;
    std::vector<std::string> profiles{{"common"}};
    std::vector<std::string> enabled_features;
};
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace git

namespace system {
struct Parse {
    std::string name;
    std::string lib_path;
    std::string inc_path;
    std::vector<std::string> profiles{{"common"}};
};
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace system

namespace local {
struct Parse {
    std::string name;
    std::string path;
    std::vector<std::string> profiles{{"common"}};
    std::vector<std::string> enabled_features;
};
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace local

namespace vcpkg {
struct Parse {
    std::string name;
    std::string triplet;
    std::string version;
    std::vector<std::string> profiles{{"common"}};
    std::vector<std::string> enabled_features;
};
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace vcpkg

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);
} // namespace catalyst::add
