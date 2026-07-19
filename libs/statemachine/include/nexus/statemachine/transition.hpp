#pragma once
#include <string_view>

namespace nexus::statemachine {
    struct Transition {
        std::string_view from;
        std::string_view to;
    };
}
