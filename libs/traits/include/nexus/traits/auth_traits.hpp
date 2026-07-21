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
