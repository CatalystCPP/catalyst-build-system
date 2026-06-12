#pragma once

namespace catalyst::utils::yaml {

/**
 * @brief Installs ryml callbacks that throw std::runtime_error on parse/access
 * errors instead of aborting the process (rapidyaml's default behavior).
 *
 * Must be called once at startup, before any ryml parsing or tree access.
 */
void installRymlErrorHandler();

} // namespace catalyst::utils::yaml
