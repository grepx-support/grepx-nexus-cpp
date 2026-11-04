/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

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
