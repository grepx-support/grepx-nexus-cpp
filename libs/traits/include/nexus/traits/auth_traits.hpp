/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/authentication/authenticator.hpp"
#include "nexus/authentication/password_authenticator.hpp"
#include "nexus/authentication/db_authenticator.hpp"

namespace nexus::traits {
    struct AuthTraits {
        using Auth = authentication::DbAuthenticator;
    };

    template<typename T>
    concept AuthTraitsConcept = requires { typename T::Auth; } &&
                                authentication::Authenticator<typename T::Auth>;

    static_assert(AuthTraitsConcept<AuthTraits>);
} // namespace nexus::traits
