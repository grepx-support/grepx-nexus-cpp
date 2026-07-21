#pragma once
#include "nexus/session/session.hpp"

namespace nexus::session {

// Durable audit trail — every login/logout, kept forever (unlike
// SessionRegistry, which only ever knows about the currently-active
// set). Deliberately a separate port from SessionRegistry: one is a
// fast in-memory gate, the other is slow-but-durable bookkeeping. A
// caller that only needs the gate (tests, e.g.) shouldn't be forced to
// implement persistence too — same ISP reasoning as splitting
// ports::ConditionalWriter out of ports::SnapshotWriter.
template <typename T>
concept LoginAudit = requires(T& audit, SessionId id, const Session& s) {
    { audit.record_login(id, s) } -> std::same_as<void>;
    { audit.record_logout(id) } -> std::same_as<void>;
};

} // namespace nexus::session
