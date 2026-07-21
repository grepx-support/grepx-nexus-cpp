#pragma once
#include "nexus/session/login_audit.hpp"
#include "nexus/session/session.hpp"
#include "nexus/session/session_registry.hpp"
#include <optional>

namespace nexus::session {

// Single entry point a connection handler actually calls. login() does
// the fast uniqueness check first (cheap, no I/O) and only touches the
// audit DB if that succeeds — a rejected duplicate-application login
// never hits SQLite at all. logout() is the mirror: drop the in-memory
// slot, then close out the audit row.
template <SessionRegistry Registry, LoginAudit Audit>
class SessionManager {
public:
    SessionManager(Registry& registry, Audit& audit) : registry_(registry), audit_(audit) {}

    std::optional<SessionId> login(const Session& s) {
        auto id = registry_.login(s);
        if (!id) return std::nullopt; // application_name already has an active session
        audit_.record_login(*id, s);
        return id;
    }

    void logout(SessionId id) {
        registry_.logout(id);
        audit_.record_logout(id);
    }

private:
    Registry& registry_;
    Audit& audit_;
};

} // namespace nexus::session
