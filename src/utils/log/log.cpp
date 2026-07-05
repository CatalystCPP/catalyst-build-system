#include "catalyst/utils/log/log.hpp"

#include <iostream>
#include <limits>

#if FF_catalyst__log_machine_info
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#endif

namespace {
std::string escapeJsonString(const std::string &input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += std::format("\\u{:04x}", static_cast<int>(c));
                } else {
                    out += c;
                }
        }
    }
    return out;
}
}

namespace catalyst {

namespace {
#if FF_catalyst__log_machine_info
std::string getHostnameImpl() {
    constexpr size_t BUFFER_SIZE = std::numeric_limits<char>::max() + 1;
    std::array<char, BUFFER_SIZE> buf{};

#ifdef _WIN32
    DWORD size = static_cast<DWORD>(buf.size());
    if (GetComputerNameA(buf.data(), &size))
        return buf.data();
#else
    if (gethostname(buf.data(), buf.size()) == 0)
        return buf.data();
#endif
    return "unknown";
}

unsigned long getPidImpl() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}
#endif
}

LogT::LogT()
    : log_file{".catalyst.log", std::ios_base::app}
#if FF_catalyst__log_machine_info
      ,
      hostname{getHostnameImpl()}, pid{getPidImpl()}
#endif
{
    auto now = std::chrono::system_clock::now();
#if FF_catalyst__uniform_logs
    log_file << generateJsonLogEvent(now, LogLevel::DBG, "begin session") << "\n";
#else
#if FF_catalyst__log_machine_info
    log_file << std::format(
        R"({{"event":"begin_session","timestamp":"{:%Y-%m-%d %H:%M:%S}","hostname":"{}","pid":{}}})",
        now,
        this->hostname,
        this->pid)
             << "\n";
#else
    log_file << std::format(R"({{"event":"begin_session","timestamp":"{:%Y-%m-%d %H:%M:%S}"}})", now) << "\n";
#endif
#endif
}

LogT::~LogT() {
    auto now = std::chrono::system_clock::now();
#if FF_catalyst__uniform_logs
    log_file << generateJsonLogEvent(now, LogLevel::DBG, "end session") << "\n";
#else
    // Destructor assumes single thread or end of life
#if FF_catalyst__log_machine_info
    log_file << std::format(R"({{"event":"end_session","timestamp":"{:%Y-%m-%d %H:%M:%S}","hostname":"{}","pid":{}}})",
                            now,
                            this->hostname,
                            this->pid)
             << "\n";
#else
    log_file << std::format(R"({{"event":"end_session","timestamp":"{:%Y-%m-%d %H:%M:%S}"}})", now) << "\n";
#endif
    if (log_file.is_open()) {
        log_file.close();
    }
#endif
}

bool LogT::isOpen() const {
    std::lock_guard<std::mutex> lock(logging_mt);
    return log_file.is_open();
}

void LogT::flush() const {
    std::lock_guard<std::mutex> lock(logging_mt);
    log_file.flush();
}

void LogT::close() const {
    std::lock_guard<std::mutex> lock(logging_mt);
    if (log_file.is_open()) {
        log_file.close();
    }
}

void LogT::logImpl(LogLevel level, const std::string &message) const {
    std::lock_guard<std::mutex> lock(logging_mt);
    if (!log_file.is_open()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    log_file << generateJsonLogEvent(now, level, message) << "\n";

    if (verbose_logging || level != LogLevel::DBG) {
        const char *color = RESET;
        switch (level) {
            case LogLevel::DBG:
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

        std::ostream &sink = (level == LogLevel::ERROR) ? std::cerr : std::cout;
        std::string time_str = std::format("{:%Y-%m-%d %H:%M:%S}", now);
        std::string log_str = std::format("[{}] {}", level, message);
        sink << time_str << " " << color << log_str << RESET << '\n';
        sink.flush();
    }
}

std::string LogT::generateJsonLogEvent(const std::chrono::system_clock::time_point &now,
                                       LogLevel level,
                                       const std::string &message) const {
#if FF_catalyst__log_machine_info
    return std::format(R"({{"timestamp":"{:%Y-%m-%d %H:%M:%S}","level":"{}","message":"{}","hostname":"{}","pid":{}}})",
                       now,
                       level,
                       escapeJsonString(message),
                       this->hostname,
                       this->pid);
#else
    return std::format(R"({{"timestamp":"{:%Y-%m-%d %H:%M:%S}","level":"{}","message":"{}"}})",
                       now,
                       level,
                       escapeJsonString(message));
#endif
}

} // namespace catalyst
