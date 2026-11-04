/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "transition.hpp"
#include <algorithm>
#include <span>
#include <string_view>

namespace nexus::statemachine {
    class TransactionTable {
    public:
        constexpr explicit
        TransactionTable(std::span<const Transition> transitions) noexcept : transitions_(transitions) {
        }

        [[nodiscard]] constexpr bool is_allowed(std::string_view from, std::string_view to) const noexcept {
            return std::ranges::any_of(transitions_, [&](const Transition &t) {
                return t.from == from && t.to == to;
            });
        }

    private:
        std::span<const Transition> transitions_;
    };
}
