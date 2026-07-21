#pragma once
#include "nexus/logging/logger.hpp"
#include "nexus/logging/timestamp.hpp"
#include <iostream>
#include <mutex>

namespace nexus::logging {

// Default implementation: "[timestamp] [LEVEL] message" — Info/Debug to
// stdout, Warn/Error to stderr. One mutex so concurrent connection
// threads (see apps/nexus_server) can't interleave partial lines.
class ConsoleLogger : public ILogger {
public:
    void log(LogLevel level, std::string_view message) override {
        std::lock_guard lock(mutex_);
        auto& out = (level == LogLevel::Warn || level == LogLevel::Error) ? std::cerr : std::cout;
        out << "[" << detail::now_timestamp() << "] [" << to_string(level) << "] "
            << message << std::endl;
    }

private:
    std::mutex mutex_;
};

} // namespace nexus::logging
