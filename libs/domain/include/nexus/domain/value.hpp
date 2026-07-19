#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace nexus::domain {

// A single cell value. monostate represents NULL.
using Value = std::variant<std::monostate, std::int64_t, double, bool, std::string>;

} // namespace nexus::domain
