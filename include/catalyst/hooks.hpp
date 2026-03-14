#pragma once
#include <expected>
#include <filesystem>
#include <string>

#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/node/node.h"

namespace catalyst::hooks {
std::expected<void, std::string> preClean(const YAML::Node &profile_comp);
std::expected<void, std::string> postClean(const YAML::Node &profile_comp);
std::expected<void, std::string> preRun(const YAML::Node &profile_comp);
std::expected<void, std::string> postRun(const YAML::Node &profile_comp);
std::expected<void, std::string> preTest(const YAML::Node &profile_comp);
std::expected<void, std::string> postTest(const YAML::Node &profile_comp);
std::expected<void, std::string> preBench(const YAML::Node &profile_comp);
std::expected<void, std::string> postBench(const YAML::Node &profile_comp);

std::expected<void, std::string> preBuild(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> postBuild(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> onBuildFailure(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> preGenerate(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> postGenerate(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> preFetch(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> postFetch(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> preClean(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> postClean(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> preRun(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> postRun(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> preTest(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> postTest(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> preBench(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> postBench(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> preLink(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> postLink(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> onCompile(const std::filesystem::path &file);
}; // namespace catalyst::hooks
