/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/logging/log_level.hpp"
#include <string_view>

namespace nexus::logging {

// Runtime-swappable on purpose — unlike Auth/Session (compile-time
// trait swaps), logging is a cross-cutting concern called from many
// unrelated places. A virtual interface + a settable global instance
// (see global_logger.hpp) means swapping the implementation later (a
// JSON logger, a file logger, one that ships to a log aggregator) never
// touches a call site, and no function up and down the stack needs a
// template parameter just to log something.
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(LogLevel level, std::string_view message) = 0;

    void debug(std::string_view msg) { log(LogLevel::Debug, msg); }
    void info(std::string_view msg)  { log(LogLevel::Info, msg); }
    void warn(std::string_view msg)  { log(LogLevel::Warn, msg); }
    void error(std::string_view msg) { log(LogLevel::Error, msg); }
};

} // namespace nexus::logging
