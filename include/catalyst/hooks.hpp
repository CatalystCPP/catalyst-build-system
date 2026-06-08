#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "catalyst/utils/yaml/configuration.hpp"

#include "yaml-cpp/node/node.h"

namespace catalyst::hooks {

std::expected<void, std::string> executeCommandHook(const YAML::Node &item, std::string_view hook_name);
std::expected<void, std::string> executeScriptHook(const YAML::Node &item, std::string_view hook_name);
std::expected<void, std::string> executeCatalystHook(const YAML::Node &item, std::string_view hook_name);
std::expected<void, std::string> executeCodegenHook(const YAML::Node &codegen_node, std::string_view hook_name);
std::vector<std::string> shellCmd(std::string_view cmd);

std::expected<void, std::string> preClean(const YAML::Node &profile_comp);
std::expected<void, std::string> postClean(const YAML::Node &profile_comp);
std::expected<void, std::string> preRun(const YAML::Node &profile_comp);
std::expected<void, std::string> postRun(const YAML::Node &profile_comp);
std::expected<void, std::string> preTest(const YAML::Node &profile_comp);
std::expected<void, std::string> postTest(const YAML::Node &profile_comp);
std::expected<void, std::string> preBench(const YAML::Node &profile_comp);
std::expected<void, std::string> postBench(const YAML::Node &profile_comp);
std::expected<void, std::string> prePack(const YAML::Node &profile_comp);
std::expected<void, std::string> postPack(const YAML::Node &profile_comp);

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
std::expected<void, std::string> prePack(const utils::yaml::Configuration &profile_comp);
std::expected<void, std::string> postPack(const utils::yaml::Configuration &profile_comp);
}; // namespace catalyst::hooks
