#include <expected>
#include <memory>
#include <string>

#include "catalyst/subcommands/doc.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::doc {

std::expected<void, std::string> action(const Parse &parse_args) {
    catalyst::logger.log(LogLevel::DEBUG, "Doc subcommand invoked.");

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

    if (engine_str == "doxygen") {
        return DerivedRunner<DocEngine::Doxygen>(parse_args, config).run();
    } else if (engine_str == "clang-doc") {
        return DerivedRunner<DocEngine::ClangDoc>(parse_args, config).run();
    } else if (engine_str == "jocasta") {
        return DerivedRunner<DocEngine::Jocasta>(parse_args, config).run();
    } else {
        return std::unexpected("Unknown documentation engine: " + engine_str);
    }
}
} // namespace catalyst::doc
