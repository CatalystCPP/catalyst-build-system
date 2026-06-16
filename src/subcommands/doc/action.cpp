#include <expected>
#include <string>

#include "catalyst/subcommands/doc.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

auto catalyst::doc::action(const Parse &parse_args) -> Result<void> {
    catalyst::logger.debug("Doc subcommand invoked.");

    catalyst::utils::yaml::Configuration config;
    try {
        config = catalyst::utils::yaml::Configuration({"common"});
    } catch (std::runtime_error &err) {
        return std::unexpected(err.what());
    }

    std::string engine_str =
        parse_args.engine.value_or(config.getString("manifest.tooling.doc.engine").value_or("doxygen"));

    engine_str.erase(
        std::remove_if(engine_str.begin(), engine_str.end(), [](unsigned char c) { return std::isspace(c); }),
        engine_str.end());

    if (engine_str == "doxygen")
        return DerivedRunner<DocEngine::Doxygen>(parse_args, config).run();
    if (engine_str == "clang-doc")
        return DerivedRunner<DocEngine::ClangDoc>(parse_args, config).run();
    if (engine_str == "jocasta")
        return DerivedRunner<DocEngine::Jocasta>(parse_args, config).run();
    return std::unexpected(
        std::format("Unknown documentation engine: {} not one of doxygen, clang-doc, or jocasta", engine_str));
}
