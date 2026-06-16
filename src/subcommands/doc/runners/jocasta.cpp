#include <expected>
#include <string>

#include "catalyst/subcommands/doc.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"

namespace catalyst::doc {

template <> Result<void> DerivedRunner<DocEngine::Jocasta>::run() {
    catalyst::logger.info("Running Jocasta documentation engine");
    catalyst::logger.warn("Jocasta engine is currently under development and not fully implemented.");

    return std::unexpected("Jocasta engine not implemented");
}

template class DerivedRunner<DocEngine::Jocasta>;

} // namespace catalyst::doc
