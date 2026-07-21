#pragma once
#include <string_view>

namespace nexus::authentication {

    template <typename T>
    concept Authenticator = requires(const T& auth, std::string_view username, std::string_view password) {
        { auth.authenticate(username, password) } -> std::same_as<bool>;
    };

} // namespace nexus::authentication