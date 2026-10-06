#pragma once
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace catalyst {

// ANSI color codes
constexpr const char *RED = "\033[31m";
constexpr const char *ORANGE = "\033[33m";
constexpr const char *BLUE = "\033[34m";
constexpr const char *PURPLE = "\033[35m";
constexpr const char *BOLD = "\033[1m";
constexpr const char *RESET = "\033[0m";
enum class LogLevel : std::uint8_t {
    DBG,   // hidden info (unless asked for opt in with -V)
    INFO,  // user facing milestones
    WARN,  // warnings
    ERROR, // errors
    EXP    // build explanations (opt in with `catalyst build --explain`)
};

/// Environment variable naming the spool file a child explanation session appends to.
/// The parent owns the Markdown report and forwards the spool once the child exits.
inline constexpr const char *EXPLAIN_SPOOL_ENV = "CATALYST_EXPLAIN_SPOOL";
/// Human-readable description of why a child explanation session was started.
inline constexpr const char *EXPLAIN_ROLE_ENV = "CATALYST_EXPLAIN_ROLE";

/// Final status recorded at the end of an explanation report.
enum class ExplainStatus : std::uint8_t { Success, Failure };

// C++20 Concept to check if type T has a .what() method returning something convertible to const char*
template <typename Error_T>
concept HasWhat = requires(const Error_T t) {
    { t.what() } -> std::convertible_to<const char *>;
};

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
        logImpl(LogLevel::DBG, message);
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

    /// Explanation record. Formatting is skipped entirely while explanations are disabled.
    template <typename... ArgsT_T> void explain(std::format_string<ArgsT_T...> fmt, ArgsT_T &&...args) const {
        if (!explainEnabled())
            return;
        std::string message = std::format(fmt, std::forward<ArgsT_T>(args)...);
        logImpl(LogLevel::EXP, message);
    }

    bool isOpen() const;
    void flush() const;
    void close() const;

    bool &getVerboseLogging() const {
        return verbose_logging;
    }

    /// Routes console logs on this thread to stderr without changing the JSONL log.
    /// Nested guards restore the previous routing when they leave scope.
    class ConsoleToStderr {
    public:
        ConsoleToStderr();
        ~ConsoleToStderr();
        ConsoleToStderr(const ConsoleToStderr &) = delete;
        ConsoleToStderr &operator=(const ConsoleToStderr &) = delete;
        ConsoleToStderr(ConsoleToStderr &&) = delete;
        ConsoleToStderr &operator=(ConsoleToStderr &&) = delete;

    private:
        bool previous;
    };

    // ---- Explanation sessions -------------------------------------------------------------

    [[nodiscard]] bool explainEnabled() const {
        return explain_enabled.load(std::memory_order_relaxed);
    }

    /// Starts an explanation session. A top-level session creates a uniquely named Markdown report
    /// in the current directory; a child session (EXPLAIN_SPOOL_ENV set) appends to its parent's spool.
    /// Calling this while a session is already active is a no-op (in-process hook dispatch).
    void beginExplainSession(std::string_view command_context) const;
    /// Writes the closing status for the active session. Subsequent calls are no-ops.
    void endExplainSession(ExplainStatus status, std::string_view first_failure = {}) const;
    /// Absolute path of the report owned by this process, if one was created.
    [[nodiscard]] std::optional<std::filesystem::path> explainReportPath() const;

    /// Package and profile context attached to subsequent explanation records.
    void setExplainContext(std::string package, std::vector<std::string> profiles) const;

    /// Environment entries a child Catalyst build must receive to forward explanations to this session.
    /// Returns an empty map when explanations are disabled. `role` describes the child.
    [[nodiscard]] std::unordered_map<std::string, std::string> explainChildEnvironment(std::string_view role) const;
    /// Appends the child's forwarded explanations (if any) to this session and removes the spool.
    void collectExplainChild(const std::unordered_map<std::string, std::string> &child_environment,
                             std::string_view role,
                             std::optional<int> exit_code) const;

    /// RAII guard naming the build phase for explanation records emitted on this thread.
    class ExplainPhase {
    public:
        explicit ExplainPhase(std::string_view phase);
        ~ExplainPhase();
        ExplainPhase(const ExplainPhase &) = delete;
        ExplainPhase &operator=(const ExplainPhase &) = delete;
        ExplainPhase(ExplainPhase &&) = delete;
        ExplainPhase &operator=(ExplainPhase &&) = delete;

    private:
        std::string previous;
    };

private:
    LogT();
    ~LogT();

    void logImpl(LogLevel level, const std::string &message) const;
    void explainImpl(const std::chrono::system_clock::time_point &now, const std::string &message) const;
    void writeReport(std::string_view text) const;
    void reportFailure(std::string_view what) const;
    std::string generateJsonLogEvent(const std::chrono::system_clock::time_point &now,
                                     LogLevel level,
                                     const std::string &message) const;

    mutable std::ofstream log_file;
    mutable std::mutex logging_mt;
    mutable bool verbose_logging = false;

    // Explanation state; guarded by logging_mt except for the atomic flag.
    mutable std::atomic<bool> explain_enabled = false;
    mutable bool explain_session_active = false;
    mutable bool explain_session_ended = false;
    mutable bool explain_is_child = false;
    mutable bool explain_styled = false;
    mutable std::FILE *explain_sink = nullptr; // report (top level) or spool (child)
    mutable std::filesystem::path explain_sink_path;
    mutable std::optional<std::filesystem::path> explain_report;
    mutable std::string explain_report_note;
    mutable std::string explain_package;
    mutable std::vector<std::string> explain_profiles;
    mutable unsigned explain_child_counter = 0;
    mutable bool explain_write_failed = false;

#if FF_catalyst__log_machine_info
    const std::string hostname;
    const unsigned long pid;
#endif
};

// Global logger instance
inline const LogT &logger = LogT::instance();

template <typename Error_T> void logException(const Error_T &err) {
    if constexpr (HasWhat<Error_T>) {
        logger.error("An unexpected error occurred: {}", err.what());
    } else {
        logger.error("An unknown exception of type '{}' occurred.", typeid(Error_T).name());
    }
}

} // namespace catalyst

template <> struct std::formatter<catalyst::LogLevel> : std::formatter<std::string_view> {
    auto format(catalyst::LogLevel level, std::format_context &ctx) const {
        std::string_view name = "UNKNOWN";
        switch (level) {
            case catalyst::LogLevel::DBG:
                name = "DEBUG";
                break;
            case catalyst::LogLevel::INFO:
                name = "INFO";
                break;
            case catalyst::LogLevel::WARN:
                name = "WARN";
                break;
            case catalyst::LogLevel::ERROR:
                name = "ERROR";
                break;
            case catalyst::LogLevel::EXP:
                name = "EXPLAIN";
                break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};
