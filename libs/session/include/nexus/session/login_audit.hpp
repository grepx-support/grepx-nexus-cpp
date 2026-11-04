/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

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
    // Startup-only: SessionRegistry is in-memory and always starts
    // empty on a fresh process, so any row this audit trail still
    // considers "logged in" at that point belongs to a connection
    // whose record_logout() never ran — the process that owned it was
    // killed (crash, OS shutdown) rather than stopped gracefully.
    // Required on every implementation, not left as an SQLite-specific
    // afterthought, because any durable LoginAudit has the same gap.
    { audit.reconcile_orphaned_sessions() } -> std::same_as<void>;
};

} // namespace nexus::session
