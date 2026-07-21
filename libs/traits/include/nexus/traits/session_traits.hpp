#pragma once
#include "nexus/session/in_memory_session_registry.hpp"
#include "nexus/session/session_registry.hpp"
#include "nexus/session/login_audit.hpp"
#include "nexus/session/sqlite_login_audit.hpp"

namespace nexus::traits {

struct SessionTraits {
    using Registry = session::InMemorySessionRegistry;
    using Audit = session::SqliteLoginAudit;
};

template <typename T>
concept SessionTraitsConcept = requires {
    typename T::Registry;
    typename T::Audit;
} && session::SessionRegistry<typename T::Registry> && session::LoginAudit<typename T::Audit>;

static_assert(SessionTraitsConcept<SessionTraits>);

} // namespace nexus::traits
