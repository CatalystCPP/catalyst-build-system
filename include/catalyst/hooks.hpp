#pragma once
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <ryml/ryml.hpp>

#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::hooks {

std::expected<void, std::string> executeCommandHook(ryml::ConstNodeRef item, std::string_view hook_name);
std::expected<void, std::string> executeScriptHook(ryml::ConstNodeRef item, std::string_view hook_name);
std::expected<void, std::string> executeCatalystHook(ryml::ConstNodeRef item, std::string_view hook_name);
std::expected<void, std::string> executeCodegenHook(ryml::ConstNodeRef codegen_node, std::string_view hook_name);
std::vector<std::string> shellCmd(std::string_view cmd);

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
