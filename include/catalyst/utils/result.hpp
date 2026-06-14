#pragma once
#include <expected>
#include <string>

namespace catalyst {
template <typename Value_T> using Result = std::expected<Value_T, std::string>;
}
