// libs/authentication/include/nexus/authentication/password_authenticator.hpp
#pragma once
#include <cstdlib>
#include <string>
#include <string_view>

namespace nexus::authentication {
    // Single shared secret, no per-user identity — username is accepted but
    // ignored. Kept mainly for tests/fallback; DbAuthenticator is what's
    // wired into AuthTraits now.
    class PasswordAuthenticator {
    public:
        PasswordAuthenticator() {
            if (const char *env = std::getenv("NEXUS_PASSWORD")) password_ = env;
        }

        [[nodiscard]] bool authenticate(std::string_view /*username*/, std::string_view password) const {
            return !password_.empty() && password == password_;
        }

    private:
        std::string password_;
    };
} // namespace nexus::authentication
