#pragma once
#include "nexus/logging/logger.hpp"
#include <memory>
#include <vector>

namespace nexus::logging {

// Forwards every log() call to a fixed set of other loggers — e.g. both
// ConsoleLogger and FileLogger at once, so switching to file logging
// doesn't mean losing the console output people are used to watching
// live. Owns its sub-loggers (unique_ptr), same lifetime rules as the
// global logger it typically gets installed as.
class CompositeLogger : public ILogger {
public:
    explicit CompositeLogger(std::vector<std::unique_ptr<ILogger>> loggers)
        : loggers_(std::move(loggers)) {}

    void log(LogLevel level, std::string_view message) override {
        for (auto& logger : loggers_) {
            logger->log(level, message);
        }
    }

private:
    std::vector<std::unique_ptr<ILogger>> loggers_;
};

} // namespace nexus::logging
