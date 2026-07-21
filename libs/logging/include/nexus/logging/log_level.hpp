#pragma once

namespace nexus::logging {

enum class LogLevel { Debug, Info, Warn, Error };

inline const char* to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

} // namespace nexus::logging
