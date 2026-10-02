#include "catalyst/utils/log/log.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t GENERATION_DIGITS = 20;
constexpr std::size_t RETAINED_INVOCATIONS = 50;
// OS locks are released even after a crash. Descriptors must not survive exec.
#ifdef _WIN32
class WinLogFileLock {
public:
    explicit WinLogFileLock(const std::filesystem::path &path, bool shared = false, bool wait = true)
        : handle{CreateFileW(path.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr,
                             OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL,
                             nullptr)} {
        if (handle == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD flags = 0;
        if (!shared) {
            flags |= LOCKFILE_EXCLUSIVE_LOCK;
        }
        if (!wait) {
            flags |= LOCKFILE_FAIL_IMMEDIATELY;
        }

        OVERLAPPED overlap{};
        locked = LockFileEx(handle, flags, 0, 1, 0, &overlap) != 0;
    }

    ~WinLogFileLock() {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }

    WinLogFileLock(const WinLogFileLock &) = delete;
    WinLogFileLock &operator=(const WinLogFileLock &) = delete;
    WinLogFileLock(WinLogFileLock &&) = delete;
    WinLogFileLock &operator=(WinLogFileLock &&) = delete;

    [[nodiscard]] bool isLocked() const {
        return locked;
    }

private:
    HANDLE handle = INVALID_HANDLE_VALUE;
    bool locked = false;
};
#else
class PosixLogFileLock {
public:
    explicit PosixLogFileLock(const std::filesystem::path &path, bool shared = false, bool wait = true)
        : handle{::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR)} {
        if (handle == -1) {
            return;
        }

        int operation = shared ? LOCK_SH : LOCK_EX;
        if (!wait) {
            operation |= LOCK_NB;
        }

        int result = flock(handle, operation);
        while (result == -1 && errno == EINTR) {
            result = flock(handle, operation);
        }
        locked = (result == 0);
    }

    ~PosixLogFileLock() {
        if (handle != -1) {
            ::close(handle);
        }
    }

    PosixLogFileLock(const PosixLogFileLock &) = delete;
    PosixLogFileLock &operator=(const PosixLogFileLock &) = delete;
    PosixLogFileLock(PosixLogFileLock &&) = delete;
    PosixLogFileLock &operator=(PosixLogFileLock &&) = delete;

    [[nodiscard]] bool isLocked() const {
        return locked;
    }

private:
    int handle = -1;
    bool locked = false;
};
#endif

#ifdef _WIN32
using LogFileLock = WinLogFileLock;
#else
using LogFileLock = PosixLogFileLock;
#endif

std::vector<std::filesystem::path> findLogGenerations(const std::filesystem::path &directory) {
    constexpr std::string_view LOG_EXTENSION = ".log";
    std::vector<std::filesystem::path> generations;
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() != LOG_EXTENSION) {
            continue;
        }

        const auto stem = entry.path().stem().string();
        if (stem.size() == GENERATION_DIGITS
            && std::ranges::all_of(stem, [](char c) { return c >= '0' && c <= '9'; })) {
            generations.push_back(entry.path());
        }
    }
    std::ranges::sort(generations);
    return generations;
}

void publishLatestLog(const std::filesystem::path &selected, const std::filesystem::path &latest) {
    namespace fs = std::filesystem;
    // A hard link works without symlink privileges on Windows too.
    const auto temporary = selected.parent_path() / "latest.tmp";
    std::error_code ignored;
    fs::remove(temporary, ignored);
    fs::create_hard_link(selected, temporary);
#ifdef _WIN32
    fs::remove(latest, ignored);
#endif
    fs::rename(temporary, latest);
}

// Requires the directory's rotation lock and generations ordered oldest first.
void pruneLogGenerations(std::span<const std::filesystem::path> generations) {
    namespace fs = std::filesystem;
    // Keep 50 completed predecessors, plus every still-active invocation.
    std::size_t completed = 0;
    for (const auto &generation : generations | std::views::reverse) {
        const auto lock_path = fs::path(generation.string() + ".lock");
        bool removed = false;
        {
            LogFileLock candidate(lock_path, false, false);
            if (candidate.isLocked()) {
                ++completed;
                if (completed > RETAINED_INVOCATIONS)
                    removed = fs::remove(generation);
            }
        }
        if (removed) {
            std::error_code ignored;
            fs::remove(lock_path, ignored);
        }
    }
}

// The caller holds the rotation lock through selection and active-lock acquisition.
std::filesystem::path selectInvocationLog(const std::filesystem::path &directory,
                                          const std::filesystem::path &latest,
                                          bool machine,
                                          std::string_view inherited) {
    namespace fs = std::filesystem;
    if (!inherited.empty()) {
        return inherited;
    }

    auto generations = findLogGenerations(directory);
    // A standalone machine call never advances the ring.
    if (machine) {
        return generations.empty() ? latest : generations.back();
    }

    unsigned long long sequence = 1;
    if (!generations.empty()) {
        sequence = std::stoull(generations.back().stem().string()) + 1;
    }
    const auto selected = directory / std::format("{:020}.log", sequence);
    // Preserve the old unbounded file once, as the oldest generation.
    if (generations.empty() && fs::is_regular_file(latest)) {
        const auto legacy = directory / "00000000000000000000.log";
        fs::rename(latest, legacy);
        generations.push_back(legacy);
    }
    {
        std::ofstream created(selected, std::ios::app);
        if (!created) {
            throw std::runtime_error("cannot create invocation log");
        }
    }
    publishLatestLog(selected, latest);
    pruneLogGenerations(generations);
    return selected;
}

void exportInvocationLog(const std::filesystem::path &selected) {
#ifdef _WIN32
    const int result = _putenv_s("CATALYST_LOG_PATH", selected.string().c_str());
#else
    const int result = setenv("CATALYST_LOG_PATH", selected.string().c_str(), 1);
#endif
    if (result != 0) {
        throw std::runtime_error("cannot export invocation log path");
    }
}

std::filesystem::path invocationLogPath() {
    namespace fs = std::filesystem;
    // Constructed before LogT completes, so this outlives its final flush.
    static std::unique_ptr<LogFileLock> active_lock;
    try {
        const bool is_machine_invoked = std::getenv("CATALYST_MACHINE") != nullptr;
        const char *inherited_env = is_machine_invoked ? std::getenv("CATALYST_LOG_PATH") : nullptr;
        const std::string_view inherited = (inherited_env != nullptr) ? inherited_env : "";

        const auto latest = fs::absolute(".catalyst.log");
        const auto directory = !inherited.empty() ? fs::path(inherited).parent_path() : fs::absolute(".catalyst.logs");
        fs::create_directories(directory);

        LogFileLock rotation_lock(directory / "rotation.lock");
        if (!rotation_lock.isLocked()) {
            throw std::runtime_error("cannot lock log directory");
        }

        const auto selected = selectInvocationLog(directory, latest, is_machine_invoked, inherited);
        active_lock = std::make_unique<LogFileLock>(selected.string() + ".lock", true);
        if (!active_lock->isLocked()) {
            throw std::runtime_error("cannot lock invocation log");
        }
        exportInvocationLog(selected);
        return selected;
    } catch (const std::exception &error) {
        // Logging setup must not prevent commands from running.
        std::println(std::cerr, "Warning: cannot initialize Catalyst log: {}", error.what());
        return {};
    }
}

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
                if (static_cast<unsigned char>(c) < ' ') {
                    out += std::format("\\u{:04x}", static_cast<int>(c));
                } else {
                    out += c;
                }
        }
    }
    return out;
}
} // namespace

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
} // namespace

LogT::LogT()
    : log_file{invocationLogPath(), std::ios_base::app}
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
    log_file.flush();
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
    // Flush each record before children or concurrent processes append theirs.
    log_file << generateJsonLogEvent(now, level, message) << '\n';
    log_file.flush();

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
    return std::format(
        R"({{"timestamp":"{:%Y-%m-%d %H:%M:%S}","level":"{}","message":"{}"}})", now, level, escapeJsonString(message));
#endif
}

} // namespace catalyst
