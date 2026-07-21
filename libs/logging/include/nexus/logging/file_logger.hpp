#pragma once
#include "nexus/logging/logger.hpp"
#include "nexus/logging/timestamp.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>

namespace nexus::logging {

// Appends "[timestamp] [LEVEL] message" lines to a file — same format
// as ConsoleLogger, just a different destination. Creates parent
// directories if they don't exist yet (e.g. a fresh checkout's first
// run, before any logs/ directory exists).
class FileLogger : public ILogger {
public:
    explicit FileLogger(const std::filesystem::path& path) {
        std::filesystem::create_directories(path.parent_path());
        out_.open(path, std::ios::app);
        if (!out_) {
            throw std::runtime_error("FileLogger: failed to open " + path.string());
        }
    }

    void log(LogLevel level, std::string_view message) override {
        std::lock_guard lock(mutex_);
        out_ << "[" << detail::now_timestamp() << "] [" << to_string(level) << "] "
             << message << std::endl;
    }

private:
    std::ofstream out_;
    std::mutex mutex_;
};

} // namespace nexus::logging
