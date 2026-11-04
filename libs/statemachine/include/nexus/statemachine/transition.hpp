/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include <string_view>

namespace nexus::statemachine {
    struct Transition {
        std::string_view from;
        std::string_view to;
    };
}
