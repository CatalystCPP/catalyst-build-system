#include "catalyst/utils/log/log.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <io.h>
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
    if (explain_sink != nullptr) {
        if (!explain_session_ended && !explain_is_child) {
            std::fputs("\n## Result\n\n- **Status:** incomplete. The process exited without recording a completion "
                       "status; explanations above may be partial.\n",
                       explain_sink);
        }
        std::fclose(explain_sink);
        explain_sink = nullptr;
    }
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
    auto now = std::chrono::system_clock::now();

    if (level == LogLevel::EXP) {
        if (!explain_enabled.load(std::memory_order_relaxed))
            return;
        // The JSONL session log is optional for explanations: stderr and the report do not depend on it.
        if (log_file.is_open()) {
            log_file << generateJsonLogEvent(now, level, message) << '\n';
            log_file.flush();
        }
        explainImpl(now, message);
        return;
    }

    if (!log_file.is_open()) {
        return;
    }

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
            case LogLevel::EXP:
                color = BOLD;
                break;
        }

        std::ostream &sink = (level == LogLevel::ERROR) ? std::cerr : std::cout;
        std::string time_str = std::format("{:%Y-%m-%d %H:%M:%S}", now);
        std::string log_str = std::format("[{}] {}", level, message);
        sink << time_str << " " << color << log_str << RESET << '\n';
        sink.flush();
    }
}

// ---- Explanation sessions -------------------------------------------------------------------

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local std::string g_explain_phase;

std::vector<std::string_view> splitLines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    while (lines.size() > 1 && lines.back().empty())
        lines.pop_back();
    return lines;
}

std::string joinProfiles(const std::vector<std::string> &profiles) {
    std::string out;
    for (const auto &profile : profiles)
        out += (out.empty() ? "" : ",") + profile;
    return out;
}

std::string indentLines(std::string_view text, std::string_view indent) {
    std::string out;
    for (auto line : splitLines(text)) {
        if (!line.empty())
            out += indent;
        out += line;
        out += '\n';
    }
    return out;
}

bool stderrIsStyled() {
    if (std::getenv("NO_COLOR") != nullptr)
        return false;
#ifdef _WIN32
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(STDERR_FILENO) != 0;
#endif
}

void unsetEnvironment(const char *name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

/// Reserves `candidate` through an exclusive `<candidate>.lock`, then creates the report exclusively.
/// Returns nullptr when the candidate is taken (lock or report already present).
std::FILE *tryCreateReport(const std::filesystem::path &candidate, std::string &error) {
    namespace fs = std::filesystem;
    const fs::path lock_path = fs::path(candidate.string() + ".lock");
    errno = 0;
    std::FILE *lock = std::fopen(lock_path.string().c_str(), "wx");
    if (lock == nullptr) {
        if (errno != EEXIST)
            error = std::format("cannot create {}: {}", lock_path.string(), std::strerror(errno));
        return nullptr; // another invocation owns this candidate, or a stale lock: skip it
    }
    std::fclose(lock);

    std::FILE *report = nullptr;
    std::error_code ec;
    if (!fs::exists(candidate, ec)) {
        errno = 0;
        report = std::fopen(candidate.string().c_str(), "wx");
        if (report == nullptr && errno != EEXIST)
            error = std::format("cannot create {}: {}", candidate.string(), std::strerror(errno));
    }
    // Release only the lock this invocation created.
    fs::remove(lock_path, ec);
    return report;
}
} // namespace

LogT::ExplainPhase::ExplainPhase(std::string_view phase) : previous(std::move(g_explain_phase)) {
    g_explain_phase = phase;
}

LogT::ExplainPhase::~ExplainPhase() {
    g_explain_phase = std::move(previous);
}

void LogT::setExplainContext(std::string package, std::vector<std::string> profiles) const {
    std::lock_guard<std::mutex> lock(logging_mt);
    explain_package = std::move(package);
    explain_profiles = std::move(profiles);
}

void LogT::reportFailure(std::string_view what) const {
    // Requires logging_mt. Warn once; stderr explanations and the build continue.
    if (!explain_write_failed) {
        explain_write_failed = true;
        explain_report_note = std::string{what};
        std::println(std::cerr,
                     "{}[WARN] Explanation report unavailable: {}. Explanations continue on stderr only.{}",
                     explain_styled ? ORANGE : "",
                     what,
                     explain_styled ? RESET : "");
    }
    if (explain_sink != nullptr) {
        std::fclose(explain_sink);
        explain_sink = nullptr;
    }
}

void LogT::writeReport(std::string_view text) const {
    // Requires logging_mt.
    if (explain_sink == nullptr)
        return;
    if (std::fwrite(text.data(), 1, text.size(), explain_sink) != text.size() || std::fflush(explain_sink) != 0)
        reportFailure(std::format("failed to write {}", explain_sink_path.string()));
}

void LogT::explainImpl(const std::chrono::system_clock::time_point &now, const std::string &message) const {
    // Requires logging_mt.
    std::string context;
    if (!explain_package.empty())
        context += explain_package + " ";
    if (!explain_profiles.empty())
        context += "[" + joinProfiles(explain_profiles) + "] ";

    const auto lines = splitLines(message);
    std::string terminal;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (explain_styled)
            terminal += BOLD;
        terminal += "[EXPLAIN] ";
        terminal += i == 0 ? context : std::string(2, ' ');
        terminal += lines[i];
        if (explain_styled)
            terminal += RESET;
        terminal += '\n';
    }
    std::cerr << terminal << std::flush;

    if (explain_sink == nullptr)
        return;
    std::string record = std::format("- `{:%H:%M:%S}Z`", std::chrono::floor<std::chrono::seconds>(now));
    if (!explain_package.empty())
        record += std::format(" **{}**", explain_package);
    if (!explain_profiles.empty())
        record += std::format(" `[{}]`", joinProfiles(explain_profiles));
    if (!g_explain_phase.empty())
        record += std::format(" _{}_", g_explain_phase);
    record += " — ";
    record += lines.front();
    record += '\n';
    if (lines.size() > 1) {
        record += "\n  ~~~~text\n";
        for (std::size_t i = 1; i < lines.size(); ++i) {
            record += "  ";
            record += lines[i];
            record += '\n';
        }
        record += "  ~~~~\n\n";
    }
    writeReport(record);
}

void LogT::beginExplainSession(std::string_view command_context) const {
    namespace fs = std::filesystem;
    std::optional<fs::path> announced;
    std::string role;
    {
        std::lock_guard<std::mutex> lock(logging_mt);
        if (explain_session_active)
            return; // in-process hook dispatch inherits the active session
        explain_session_active = true;
        explain_styled = stderrIsStyled();

        const char *spool_env = std::getenv(EXPLAIN_SPOOL_ENV);
        if (spool_env != nullptr && *spool_env != '\0') {
            // Child session: forward records through the parent's spool; the parent owns the report.
            explain_is_child = true;
            explain_sink_path = spool_env;
            const char *role_env = std::getenv(EXPLAIN_ROLE_ENV);
            role = role_env != nullptr ? role_env : "child build";
            explain_sink = std::fopen(explain_sink_path.string().c_str(), "a");
            if (explain_sink == nullptr)
                reportFailure(std::format("cannot open forwarding spool {}", explain_sink_path.string()));
            // Unrelated processes started from this child (hooks) must not share the spool.
            unsetEnvironment(EXPLAIN_SPOOL_ENV);
            unsetEnvironment(EXPLAIN_ROLE_ENV);
        } else {
            std::error_code ec;
            const fs::path directory = fs::current_path(ec);
            const auto started = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
            const std::string stem = std::format("catalyst_explain_{:%Y-%m-%d_%H-%M-%S}", started);
            std::string error;
            constexpr unsigned MAX_SUFFIX = 10000;
            for (unsigned suffix = 0; suffix < MAX_SUFFIX && explain_sink == nullptr && error.empty(); ++suffix) {
                const fs::path candidate =
                    directory / (suffix == 0 ? stem + ".md" : std::format("{}.{}.md", stem, suffix));
                explain_sink = tryCreateReport(candidate, error);
                if (explain_sink != nullptr) {
                    explain_sink_path = candidate;
                    explain_report = candidate;
                    if (suffix != 0)
                        explain_report_note =
                            std::format("Filename collision: {}.md and earlier suffixes were taken; using suffix .{}.",
                                        stem,
                                        suffix);
                }
            }
            if (explain_sink == nullptr) {
                reportFailure(error.empty() ? std::string{"no free report filename"} : error);
            } else {
                std::string header = "# Catalyst build explanation\n\n";
                header += std::format("- **Invocation time:** {:%Y-%m-%d %H:%M:%S} UTC\n", started);
                header += std::format("- **Working directory:** `{}`\n", directory.string());
                header += std::format("- **Command:** `{}`\n", command_context);
                header += std::format("- **Report:** `{}`\n", explain_sink_path.string());
                header += "\n> [!WARNING]\n"
                          "> This report is not sanitized. Configuration snapshots, feature values, URLs and "
                          "expanded hook commands may contain sensitive information. Do not publish it without "
                          "review.\n\n## Explanations\n\n";
                writeReport(header);
                announced = explain_report;
            }
        }
        explain_enabled.store(true, std::memory_order_relaxed);
    }
    if (announced)
        explain("Report: {}", announced->string());
    if (!role.empty())
        explain("Child explanation session ({}); records are forwarded to the parent's report.", role);
}

void LogT::endExplainSession(ExplainStatus status, std::string_view first_failure) const {
    std::lock_guard<std::mutex> lock(logging_mt);
    if (!explain_session_active || explain_session_ended)
        return;
    explain_session_ended = true;
    if (explain_is_child) {
        if (explain_sink != nullptr) {
            std::fclose(explain_sink);
            explain_sink = nullptr;
        }
        return;
    }
    std::string footer = "\n## Result\n\n";
    footer += std::format("- **Status:** {}\n", status == ExplainStatus::Success ? "success" : "failure");
    if (!first_failure.empty())
        footer += std::format("- **First failure:**\n\n{}\n", indentLines(first_failure, "      "));
    if (explain_report)
        footer += std::format("- **Report:** `{}`\n", explain_report->string());
    if (!explain_report_note.empty())
        footer += std::format("- **Report notes:** {}\n", explain_report_note);
    writeReport(footer);
    std::cerr << std::format("{}[EXPLAIN] Build {}. Report: {}{}\n",
                             explain_styled ? BOLD : "",
                             status == ExplainStatus::Success ? "succeeded" : "failed",
                             explain_report ? explain_report->string() : std::string{"<not written>"},
                             explain_styled ? RESET : "")
              << std::flush;
    if (explain_sink != nullptr) {
        std::fclose(explain_sink);
        explain_sink = nullptr;
    }
}

std::optional<std::filesystem::path> LogT::explainReportPath() const {
    std::lock_guard<std::mutex> lock(logging_mt);
    return explain_report;
}

std::unordered_map<std::string, std::string> LogT::explainChildEnvironment(std::string_view role) const {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lock(logging_mt);
    if (!explain_enabled.load(std::memory_order_relaxed))
        return {};
    fs::path base = explain_sink_path;
    if (base.empty()) {
        std::error_code ec;
        base = fs::current_path(ec) / "catalyst_explain";
    }
#ifdef _WIN32
    const unsigned long process = GetCurrentProcessId();
#else
    const unsigned long process = static_cast<unsigned long>(getpid());
#endif
    const auto spool = std::format("{}.child-{}-{}.part", base.string(), process, ++explain_child_counter);
    std::error_code ignored;
    fs::remove(spool, ignored);
    return {{EXPLAIN_SPOOL_ENV, spool}, {EXPLAIN_ROLE_ENV, std::string{role}}};
}

void LogT::collectExplainChild(const std::unordered_map<std::string, std::string> &child_environment,
                               std::string_view role,
                               std::optional<int> exit_code) const {
    namespace fs = std::filesystem;
    const auto spool_it = child_environment.find(EXPLAIN_SPOOL_ENV);
    if (spool_it == child_environment.end())
        return;
    const fs::path spool = spool_it->second;
    std::string forwarded;
    {
        std::ifstream in(spool, std::ios::binary);
        if (in)
            forwarded.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
    }
    std::error_code ignored;
    fs::remove(spool, ignored);

    const std::string status = exit_code ? std::format("exit code {}", *exit_code) : std::string{"not started"};
    std::lock_guard<std::mutex> lock(logging_mt);
    std::string header = std::format("{}[EXPLAIN] {}{} explanation records from child {} ({}).{}\n",
                                     explain_styled ? BOLD : "",
                                     explain_package.empty() ? "" : explain_package + " ",
                                     forwarded.empty() ? "No forwarded" : "Collected forwarded",
                                     role,
                                     status,
                                     explain_styled ? RESET : "");
    std::cerr << header << std::flush;
    if (forwarded.empty()) {
        writeReport(std::format("- **Child {}** ({}): no explanation records were forwarded.\n", role, status));
        return;
    }
    writeReport(std::format("- **Child {}** ({}), forwarded records:\n\n{}\n", role, status, indentLines(forwarded, "  ")));
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
