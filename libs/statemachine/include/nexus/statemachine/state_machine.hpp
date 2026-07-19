#pragma once
#include <string_view>

#include "transition_table.hpp"

namespace nexus::statemachine {
    class StateMachine {
    public:
        constexpr explicit StateMachine(TransactionTable table) noexcept : table_(table) {
        }

        [[nodiscard]] constexpr bool is_allowed(std::string_view from, std::string_view to) const noexcept {
            return table_.is_allowed(from, to);
        }

    private:
        TransactionTable table_;
    };
}

