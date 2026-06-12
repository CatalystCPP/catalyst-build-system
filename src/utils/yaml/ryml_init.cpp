#include "catalyst/utils/yaml/ryml_init.hpp"

#include <format>
#include <stdexcept>
#include <string_view>

#include <ryml/ryml.hpp>

namespace catalyst::utils::yaml {

namespace {

[[noreturn]] void
throwingErrorCallback(const char *msg, size_t msg_len, ryml::Location location, void * /*user_data*/) {
    std::string_view message{msg, msg_len};
    // ryml messages often end with a newline; trim it for clean log/what() text.
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
        message.remove_suffix(1);

    std::string_view name =
        location.name.empty() ? std::string_view{} : std::string_view{location.name.str, location.name.len};

    if (name.empty())
        throw std::runtime_error(std::format("YAML error at {}:{}: {}", location.line, location.col, message));
    throw std::runtime_error(std::format("YAML error at {}:{}:{}: {}", name, location.line, location.col, message));
}

} // namespace

void installRymlErrorHandler() {
    // Null alloc/free keep ryml's default implementations.
    ryml::set_callbacks(ryml::Callbacks{nullptr, nullptr, nullptr, throwingErrorCallback});
}

} // namespace catalyst::utils::yaml
