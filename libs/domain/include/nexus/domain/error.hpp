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
