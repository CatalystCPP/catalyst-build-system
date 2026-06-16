#pragma once
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <ryml/ryml.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::hooks {

Result<void> executeCommandHook(ryml::ConstNodeRef item, std::string_view hook_name);
Result<void> executeScriptHook(ryml::ConstNodeRef item, std::string_view hook_name);
Result<void> executeCatalystHook(ryml::ConstNodeRef item, std::string_view hook_name);
Result<void> executeCodegenHook(ryml::ConstNodeRef codegen_node, std::string_view hook_name);
std::vector<std::string> shellCmd(std::string_view cmd);

Result<void> preBuild(const utils::yaml::Configuration &profile_comp);
Result<void> postBuild(const utils::yaml::Configuration &profile_comp);
Result<void> onBuildFailure(const utils::yaml::Configuration &profile_comp);
Result<void> preGenerate(const utils::yaml::Configuration &profile_comp);
Result<void> postGenerate(const utils::yaml::Configuration &profile_comp);
Result<void> preFetch(const utils::yaml::Configuration &profile_comp);
Result<void> postFetch(const utils::yaml::Configuration &profile_comp);
Result<void> preClean(const utils::yaml::Configuration &profile_comp);
Result<void> postClean(const utils::yaml::Configuration &profile_comp);
Result<void> preRun(const utils::yaml::Configuration &profile_comp);
Result<void> postRun(const utils::yaml::Configuration &profile_comp);
Result<void> preTest(const utils::yaml::Configuration &profile_comp);
Result<void> postTest(const utils::yaml::Configuration &profile_comp);
Result<void> preBench(const utils::yaml::Configuration &profile_comp);
Result<void> postBench(const utils::yaml::Configuration &profile_comp);
Result<void> prePack(const utils::yaml::Configuration &profile_comp);
Result<void> postPack(const utils::yaml::Configuration &profile_comp);
}; // namespace catalyst::hooks
