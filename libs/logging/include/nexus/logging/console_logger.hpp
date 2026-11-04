/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/logging/logger.hpp"
#include "nexus/logging/timestamp.hpp"
#include <iostream>
#include <mutex>

namespace nexus::logging {

// Default implementation: "[timestamp] [LEVEL] [tid=...] message" —
// Info/Debug to stdout, Warn/Error to stderr. One mutex so concurrent
// connection threads (see apps/nexus_server) can't interleave partial
// lines. The thread id is here (not something each call site has to
// pass in) because nexus_server is one thread per connection — it's
// always available and always means the same thing, so it belongs in
// the shared format, not duplicated at every log() call.
class ConsoleLogger : public ILogger {
public:
    void log(LogLevel level, std::string_view message) override {
        std::lock_guard lock(mutex_);
        auto& out = (level == LogLevel::Warn || level == LogLevel::Error) ? std::cerr : std::cout;
        out << "[" << detail::now_timestamp() << "] [" << to_string(level) << "] "
            << "[tid=" << detail::current_thread_id() << "] "
            << message << std::endl;
    }

private:
    std::mutex mutex_;
};

} // namespace nexus::logging
