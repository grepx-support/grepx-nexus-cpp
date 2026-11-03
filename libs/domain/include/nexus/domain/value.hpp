/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace nexus::domain {

// A single cell value. monostate represents NULL.
using Value = std::variant<std::monostate, std::int64_t, double, bool, std::string>;

} // namespace nexus::domain
