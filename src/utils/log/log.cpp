#include "catalyst/utils/log/log.hpp"

#include <iostream>

#include <nlohmann/json.hpp>

namespace catalyst {

const char *toString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARN:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
    }
    return "UNKNOWN";
}

LogT::LogT() : log_file{".catalyst.log", std::ios_base::app} {
    auto now = std::chrono::system_clock::now();
    nlohmann::json j;
    j["event"] = "begin_session";
    j["timestamp"] = std::format("{:%Y-%m-%d %H:%M:%S}", now);
    log_file << j.dump() << "\n";
}

LogT::~LogT() {
    auto now = std::chrono::system_clock::now();
    // Destructor assumes single thread or end of life
    nlohmann::json j;
    j["event"] = "end_session";
    j["timestamp"] = std::format("{:%Y-%m-%d %H:%M:%S}", now);
    log_file << j.dump() << "\n";
    if (log_file.is_open()) {
        log_file.close();
    }
}

bool LogT::isOpen() const {
    std::lock_guard<std::mutex> lock(log_file_mutex);
    return log_file.is_open();
}

void LogT::flush() const {
    std::lock_guard<std::mutex> lock(log_file_mutex);
    log_file.flush();
}

void LogT::close() const {
    std::lock_guard<std::mutex> lock(log_file_mutex);
    if (log_file.is_open()) {
        log_file.close();
    }
}

void LogT::logImpl(LogLevel level, const std::string &message) const {
    std::lock_guard<std::mutex> lock(log_file_mutex);
    if (!log_file.is_open()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    log_file << generateJsonLogEvent(now, level, message) << "\n";

    if (verbose_logging || level != LogLevel::DEBUG) {
        const char *color = RESET;
        switch (level) {
            case LogLevel::DEBUG:
                color = PURPLE;
                break;
            case LogLevel::INFO:
                color = BLUE;
                break;
            case LogLevel::WARN:
                color = ORANGE;
                break;
            case LogLevel::ERROR:
                color = RED;
                break;
        }

        if (level == LogLevel::ERROR) {
            std::lock_guard<std::mutex> lock{stdio_mutex};
            std::cerr << std::format("[{:%Y-%m-%d %H:%M:%S}] ", now) << color
                      << std::format("[{}] {}", toString(level), message) << RESET << std::endl;
        } else {
            std::lock_guard<std::mutex> lock{stdio_mutex};
            std::cout << std::format("[{:%Y-%m-%d %H:%M:%S}] ", now) << color
                      << std::format("[{}] {}", toString(level), message) << RESET << std::endl;
        }
    }
}

std::string LogT::generateJsonLogEvent(const std::chrono::system_clock::time_point &now,
                                       LogLevel level,
                                       const std::string &message) {
    nlohmann::json j;
    j["timestamp"] = std::format("{:%Y-%m-%d %H:%M:%S}", now);
    j["level"] = toString(level);
    j["message"] = message;
    return j.dump();
}

} // namespace catalyst
