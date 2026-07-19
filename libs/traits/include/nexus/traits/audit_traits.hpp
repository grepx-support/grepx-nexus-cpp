#pragma once
#include "nexus/infra/audit_log.hpp"
#include "nexus/ports/audit_writer.hpp"

namespace nexus::traits {

struct AuditTraits {
    using Log = infra::AuditLog;
};

template <typename T>
concept AuditTraitsConcept = requires {
    typename T::Log;
} && ports::AuditWriter<typename T::Log>;

static_assert(AuditTraitsConcept<AuditTraits>);

} // namespace nexus::traits
