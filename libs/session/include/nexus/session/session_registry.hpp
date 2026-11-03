/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/session/session.hpp"
#include <optional>

namespace nexus::session {

// Fast, in-process uniqueness gate — no I/O, so checking/holding it
// never adds latency to the request path. nullopt from login() means
// "application_name already has an active session". Swappable later
// (e.g. a Redis-backed registry for a multi-process deployment) without
// touching anything upstream — same separation-of-concerns pattern as
// ports::Authenticator.
template <typename T>
concept SessionRegistry = requires(T& reg, const Session& s, SessionId id) {
    { reg.login(s) } -> std::same_as<std::optional<SessionId>>;
    { reg.logout(id) } -> std::same_as<void>;
};

} // namespace nexus::session
