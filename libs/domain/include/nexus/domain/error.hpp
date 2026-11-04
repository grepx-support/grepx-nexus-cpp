/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include <expected>

namespace nexus::domain {

enum class Error {
    TableNotFound,
    RowNotFound,
    ParseError,
    Unauthorized,
    DuplicateSubscription,
    VersionConflict,
    InvalidTransition,
};

template <typename T>
using Expected = std::expected<T, Error>;

} // namespace nexus::domain
