#pragma once
#include <chrono>
#include <format>
#include <fstream>
#include <mutex>
#include <string>

// ANSI color codes
inline const char *const RED = "\033[31m";
inline const char *const ORANGE = "\033[33m";
inline const char *const BLUE = "\033[34m";
inline const char *const PURPLE = "\033[35m";
inline const char *const RESET = "\033[0m";

namespace catalyst {
enum class LogLevel : std::uint8_t {
    DEBUG, // hidden info (unless asked for opt in with -V)
    INFO,  // user facing milestones
    WARN,  // warnings
    ERROR  // errors
};

// Helper function to convert LogLevel to string
const char *toString(LogLevel level);

class LogT {
public:
    static LogT &instance() {
        static LogT logger_instance;
        return logger_instance;
    }

    LogT(const LogT &) = delete;
    LogT &operator=(const LogT &) = delete;
    LogT(LogT &&) = delete;
    LogT &operator=(LogT &&) = delete;

    template <typename... ArgsT_T> void debug(std::format_string<ArgsT_T...> fmt, ArgsT_T &&...args) const {
        std::string message = std::format(fmt, std::forward<ArgsT_T>(args)...);
        logImpl(LogLevel::DEBUG, message);
    }

    template <typename... ArgsT_T> void info(std::format_string<ArgsT_T...> fmt, ArgsT_T &&...args) const {
        std::string message = std::format(fmt, std::forward<ArgsT_T>(args)...);
        logImpl(LogLevel::INFO, message);
    }

    template <typename... ArgsT_T> void warn(std::format_string<ArgsT_T...> fmt, ArgsT_T &&...args) const {
        std::string message = std::format(fmt, std::forward<ArgsT_T>(args)...);
        logImpl(LogLevel::WARN, message);
    }

    template <typename... ArgsT_T> void error(std::format_string<ArgsT_T...> fmt, ArgsT_T &&...args) const {
        std::string message = std::format(fmt, std::forward<ArgsT_T>(args)...);
        logImpl(LogLevel::ERROR, message);
    }

    bool isOpen() const;
    void flush() const;
    void close() const;

    bool &getVerboseLogging() const {
        return verbose_logging;
    }

private:
    LogT();
    ~LogT();

    void logImpl(LogLevel level, const std::string &message) const;
    std::string generateJsonLogEvent(const std::chrono::system_clock::time_point &now,
                                     LogLevel level,
                                     const std::string &message) const;

    mutable std::ofstream log_file;
    mutable std::mutex logging_mt;
    mutable bool verbose_logging = false;

#if FF_catalyst__log_machine_info
    const std::string hostname;
    const unsigned long pid;
#endif
};

// Global logger instance
inline const LogT &logger = LogT::instance();

} // namespace catalyst
