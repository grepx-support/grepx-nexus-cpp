/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include <string_view>

namespace nexus::authentication {

    template <typename T>
    concept Authenticator = requires(const T& auth, std::string_view username, std::string_view password) {
        { auth.authenticate(username, password) } -> std::same_as<bool>;
        { auth.can_write(username, username) } -> std::same_as<bool>;   // (username, table): may this account write it?
    };

} // namespace nexus::authentication