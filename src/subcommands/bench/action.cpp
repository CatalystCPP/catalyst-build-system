#include <expected>
#include <string>

#include "catalyst/subcommands/bench.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::bench {
std::expected<void, std::string> action(const Parse &/*parse_args*/) {
    catalyst::logger.log(LogLevel::ERROR, "Not Implemented yet");
    return std::unexpected("Not Implemented yet");
}
} // namespace catalyst::bench
