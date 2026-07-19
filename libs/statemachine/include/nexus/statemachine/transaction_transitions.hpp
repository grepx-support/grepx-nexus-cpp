#pragma once
#include "state_machine.hpp"
#include "transition.hpp"
#include "transition_table.hpp"
#include <array>

namespace nexus::statemachine {
    inline constexpr std::array kTransactionTransitions = {
        Transition{"NEW", "CLAIMED"},
        Transition{"NEW", "REJECTED"},
        Transition{"CLAIMED", "ACCEPTED"},
        Transition{"CLAIMED", "REJECTED"},
        Transition{"CLAIMED", "NEW"}, // lease expiry reclaim
        Transition{"ACCEPTED", "PROCESSING"},
        Transition{"ACCEPTED", "REJECTED"},
        Transition{"PROCESSING", "COMPLETED"},
        Transition{"PROCESSING", "FAILED"},
    };

    inline constexpr StateMachine transition_state_machine() {
        return StateMachine{TransactionTable{kTransactionTransitions}};
    }
}
