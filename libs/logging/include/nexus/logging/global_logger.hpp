/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/logging/console_logger.hpp"
#include <memory>

namespace nexus::logging {

// The single access point every call site uses:
//   nexus::logging::log().info("message");
// To swap the implementation later (e.g. a file logger, a JSON logger,
// one that ships to an aggregator), call set_logger() once at startup —
// nothing else in the codebase changes.
inline std::unique_ptr<ILogger>& logger_instance() {
    static std::unique_ptr<ILogger> instance = std::make_unique<ConsoleLogger>();
    return instance;
}

inline ILogger& log() { return *logger_instance(); }

inline void set_logger(std::unique_ptr<ILogger> logger) {
    logger_instance() = std::move(logger);
}

} // namespace nexus::logging
