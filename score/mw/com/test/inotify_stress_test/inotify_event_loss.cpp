/*******************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

/// \brief Minimal, self-contained reproducer for an inotify event-loss/reordering bug observed on QNX.
///
/// It only depends on the C/C++ standard library and plain POSIX APIs
/// (fork(), pipe(), the raw <sys/inotify.h> syscalls) — no application-specific infrastructure at all.
///
/// Sequence, repeated for a configurable number of cycles:
///   1. (Once, at start-up) the parent process ("controller") creates the directory to be watched, opens
///      an inotify instance, adds a watch for IN_CREATE/IN_DELETE on it, and starts a background thread
///      that continuously drains inotify events into a bookkeeping structure: every create/delete/
///      IN_Q_OVERFLOW event, in the exact order it was observed.
///   2. The controller forks a child process ("event producer"). The child creates kFileCount files with
///      consecutive, zero-padded names in the watched directory (in ascending index order), then removes
///      all of them again (again in ascending index order). Once done, it writes a single byte to a pipe
///      shared with the parent and exits.
///   3. The controller blocks on that pipe until the producer signals completion (and reaps it via
///      waitpid()).
///   4. The controller waits a further, configurable grace period (kCheckDelay, default 500ms) for any
///      still in-flight notifications to be dispatched, then analyzes the recorded arrival sequence (see
///      AnalyzeArrivalSequence()).
///   5. If a genuine violation is found the controller logs details and the whole test process terminates
///      with a non-zero exit code. Otherwise, it resets the bookkeeping and starts the next cycle.
///
/// This models a real production issue: some processes watching many inotify events occasionally miss one.
/// Losing notifications under load is not, by itself, considered a bug here — as long as the kernel's own
/// IN_Q_OVERFLOW indicator correctly signals that its event queue overflowed at that point. What IS a bug,
/// and what this test actually fails on, is either (a) notifications arriving out of order, or (b)
/// notifications going missing WITHOUT IN_Q_OVERFLOW ever being raised to explain the gap.

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <charconv>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{

#if defined(__QNXNTO__)
const std::string kDefaultBaseDir{"/tmp_discovery/inotify_event_loss_test"};
#else
const std::string kDefaultBaseDir{"/tmp/inotify_event_loss_test"};
#endif

constexpr std::size_t kDefaultFileCount{500U};
constexpr std::size_t kDefaultCycles{20U};
constexpr std::chrono::milliseconds kDefaultCheckDelay{500U};

/// Prefix of the files created/removed by the producer, followed by a zero-padded, consecutive index.
const std::string kFilePrefix{"evt_file_"};

std::string FileName(const std::size_t index)
{
    std::ostringstream oss;
    oss << kFilePrefix << std::setw(6) << std::setfill('0') << index;
    return oss.str();
}

/// \brief Runtime configuration, parsed from the command line.
struct Config
{
    std::string base_dir{kDefaultBaseDir};
    std::size_t file_count{kDefaultFileCount};
    std::size_t cycles{kDefaultCycles};
    std::chrono::milliseconds check_delay{kDefaultCheckDelay};
};

void PrintUsage(const char* const program_name)
{
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  --cycles N           Number of create/delete cycles to run (default: " << kDefaultCycles << ")\n"
              << "  --file-count N       Number of files created (then removed) per cycle (default: "
              << kDefaultFileCount << ")\n"
              << "  --check-delay-ms N   Grace period after a cycle ends before verifying notifications "
                 "(default: "
              << kDefaultCheckDelay.count() << ")\n"
              << "  --base-dir PATH      Directory to create and watch (default: " << kDefaultBaseDir << ")\n"
              << "  --help               Print this message and exit\n";
}

/// \brief Minimal, dependency-free command-line parser for this reproducer's few options.
std::optional<Config> ParseArguments(const int argc, char** const argv)
{
    Config config{};

    auto parse_size = [](const std::string_view text, std::size_t& out) {
        const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
        return (result.ec == std::errc{}) && (result.ptr == (text.data() + text.size()));
    };

    for (int i{1}; i < argc; ++i)
    {
        const std::string_view arg{argv[i]};
        const bool has_next{(i + 1) < argc};

        if (arg == "--help")
        {
            PrintUsage(argv[0]);
            return std::nullopt;
        }
        if ((arg == "--cycles") && has_next)
        {
            if (!parse_size(argv[++i], config.cycles))
            {
                std::cerr << "Invalid --cycles value" << std::endl;
                return std::nullopt;
            }
        }
        else if ((arg == "--file-count") && has_next)
        {
            if (!parse_size(argv[++i], config.file_count))
            {
                std::cerr << "Invalid --file-count value" << std::endl;
                return std::nullopt;
            }
        }
        else if ((arg == "--check-delay-ms") && has_next)
        {
            std::size_t value{0U};
            if (!parse_size(argv[++i], value))
            {
                std::cerr << "Invalid --check-delay-ms value" << std::endl;
                return std::nullopt;
            }
            config.check_delay = std::chrono::milliseconds{value};
        }
        else if ((arg == "--base-dir") && has_next)
        {
            config.base_dir = argv[++i];
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
            PrintUsage(argv[0]);
            return std::nullopt;
        }
    }

    if (config.file_count == 0U)
    {
        std::cerr << "--file-count must be >= 1" << std::endl;
        return std::nullopt;
    }

    return config;
}

/// \brief One observation drained from inotify, in the exact order the reader thread saw it: a file's
///        create/delete event, or the kernel's own IN_Q_OVERFLOW indicator (queue overflowed, events were
///        dropped around this point). \p index is meaningless for kOverflow.
enum class ArrivalKind : std::uint8_t
{
    kCreate,
    kDelete,
    kOverflow,
};

struct ArrivalRecord
{
    ArrivalKind kind;
    std::size_t index;  // this is the index extracted from the file-name, which is later checked against the position
                        // of ArrivalRecord in the arrival_sequence vector
};

/// \brief Per-cycle bookkeeping, shared between the main thread (which resets it at the start of a cycle
///        and inspects it at the end) and the background reader thread (which continuously appends to it
///        as events arrive). All that matters for verification lives in \c arrival_sequence — every
///        create/delete/overflow event, in the exact order it was observed.
struct Bookkeeping
{
    std::mutex mutex{};
    std::size_t file_count{0U};
    std::vector<ArrivalRecord> arrival_sequence{};
};

void ResetBookkeeping(Bookkeeping& bookkeeping, const std::size_t file_count)
{
    const std::lock_guard<std::mutex> lock{bookkeeping.mutex};
    bookkeeping.file_count = file_count;
    bookkeeping.arrival_sequence.clear();
    bookkeeping.arrival_sequence.reserve(2U * file_count);
}

/// \brief Parses the file index out of an inotify event's file name (as produced by FileName()).
///        Returns std::nullopt if \p name doesn't match the expected "<prefix><digits>" pattern.
std::optional<std::size_t> ParseFileIndex(const std::string_view name)
{
    if (name.size() <= kFilePrefix.size())
    {
        return std::nullopt;
    }
    if (name.substr(0U, kFilePrefix.size()) != kFilePrefix)
    {
        return std::nullopt;
    }
    const auto digits = name.substr(kFilePrefix.size());
    std::size_t value{0U};
    const auto parse_result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if ((parse_result.ec != std::errc{}) || (parse_result.ptr != (digits.data() + digits.size())))
    {
        return std::nullopt;
    }
    return value;
}

/// \brief Continuously drains inotify events (raw read() on the inotify fd, parsing the kernel's
///        `struct inotify_event` records directly) into \p bookkeeping until the fd is closed (process
///        shutdown) or read() otherwise fails/returns EOF. Runs for the whole lifetime of the process, so a
///        burst of events arriving faster than the main thread cycles through its steps is never missed.
void ReaderThreadFunc(const int inotify_fd, Bookkeeping& bookkeeping)
{
    // Large enough for many simultaneous events; each event is sizeof(struct inotify_event) + name length
    // (rounded up), and file names here are short and fixed-length.
    constexpr std::size_t kBufferSize{64U * 1024U};
    std::vector<char> buffer(kBufferSize);

    while (true)
    {
        const ssize_t bytes_read{::read(inotify_fd, buffer.data(), buffer.size())};
        if (bytes_read <= 0)
        {
            // fd closed (shutdown) or a genuine OS error — either way, done.
            return;
        }

        const std::lock_guard<std::mutex> lock{bookkeeping.mutex};
        std::size_t offset{0U};
        while (offset < static_cast<std::size_t>(bytes_read))
        {
            const auto* const event = reinterpret_cast<const struct inotify_event*>(buffer.data() + offset);

            if ((event->mask & IN_Q_OVERFLOW) != 0U)
            {
                bookkeeping.arrival_sequence.push_back({ArrivalKind::kOverflow, 0U});
                std::cerr << "inotify reported IN_Q_OVERFLOW — the kernel's event queue overflowed and "
                             "dropped events"
                          << std::endl;
            }

            if (event->len > 0U)
            {
                const std::string_view name{event->name};
                const auto index = ParseFileIndex(name);
                if (index.has_value() && (*index < bookkeeping.file_count))
                {
                    if ((event->mask & IN_CREATE) != 0U)
                    {
                        bookkeeping.arrival_sequence.push_back({ArrivalKind::kCreate, *index});
                    }
                    if ((event->mask & IN_DELETE) != 0U)
                    {
                        bookkeeping.arrival_sequence.push_back({ArrivalKind::kDelete, *index});
                    }
                }
            }

            offset += sizeof(struct inotify_event) + event->len;
        }
    }
}

/// \brief Analyzes \p arrival_sequence (a consistent snapshot, taken under the bookkeeping mutex) for
///        violations of the expected delivery order and for *unexplained* (silent) notification loss.
///
/// Background: it is not, by itself, a bug for the watcher to miss create/delete notifications under
/// heavy load — as long as the kernel's IN_Q_OVERFLOW indicator correctly signals that its event queue
/// overflowed and events were dropped at that point. What IS a bug is either (a) notifications arriving
/// out of order, or (b) notifications going missing WITHOUT IN_Q_OVERFLOW ever being raised to explain it.
///
/// Since the producer creates all file_count files (in ascending index order), then removes all of them
/// (again in ascending index order), a healthy run's arrival_sequence — with any IN_Q_OVERFLOW markers
/// removed — must read: create_0, create_1, ..., create_{N-1}, delete_0, delete_1, ..., delete_{N-1}. This
/// function walks the actual arrival_sequence once, tracking the next expected create/delete index:
///   - An index that matches the next expected one is always fine.
///   - An index *behind* the next expected one (a repeat, or a step backwards) is always a reordering bug —
///     no IN_Q_OVERFLOW can excuse the kernel re-delivering (or delivering out of sequence) an event.
///   - An index *ahead* of the next expected one (a gap) is only acceptable if at least one IN_Q_OVERFLOW
///     was observed since the last create (respectively delete) update — i.e. the kernel told us truthfully
///     that it dropped something right around here. Otherwise it's a silently lost notification: a bug.
///   - A create arriving after the first delete is always a phase-ordering bug, regardless of overflow.
/// The same "was there an IN_Q_OVERFLOW since the last update" check is repeated at the very end for any
/// trailing files whose create/delete was never observed at all (covers losses right at the tail, where no
/// later same-type event exists to reveal the gap by comparison).
///
/// \return An empty string if no violation was detected; otherwise a human-readable description of the
///         first violation found.
std::string AnalyzeArrivalSequence(const std::vector<ArrivalRecord>& arrival_sequence, const std::size_t file_count)
{
    std::optional<std::size_t> last_create_index{};
    std::optional<std::size_t> last_delete_index{};
    bool deletes_started{false};
    // Set by an IN_Q_OVERFLOW record; consumed (reset) only by the next update of the *same* stream — so
    // an overflow can still explain a later gap in the other stream even after this stream has moved on.
    bool overflow_since_last_create{false};
    bool overflow_since_last_delete{false};

    for (const auto& record : arrival_sequence)
    {
        if (record.kind == ArrivalKind::kOverflow)
        {
            overflow_since_last_create = true;
            overflow_since_last_delete = true;
            continue;
        }

        if (record.kind == ArrivalKind::kCreate)
        {
            if (deletes_started)
            {
                std::ostringstream oss;
                oss << "create notification for file index " << record.index
                    << " arrived after the first delete notification (expected all creates before any "
                       "delete) — this is not explainable by IN_Q_OVERFLOW";
                return oss.str();
            }

            const std::size_t expected_next{last_create_index.has_value() ? (*last_create_index + 1U) : 0U};
            if (record.index < expected_next)
            {
                std::ostringstream oss;
                oss << "create notification for file index " << record.index << " arrived out of order (after index "
                    << *last_create_index << ")";
                return oss.str();
            }
            if ((record.index > expected_next) && (!overflow_since_last_create))
            {
                std::ostringstream oss;
                oss << "silent loss: create notification(s) for file index " << expected_next
                    << (record.index - 1U > expected_next ? (".." + std::to_string(record.index - 1U)) : "")
                    << " never arrived, and no IN_Q_OVERFLOW was signaled to explain the gap";
                return oss.str();
            }
            last_create_index = record.index;
            overflow_since_last_create = false;
        }
        else  // ArrivalKind::kDelete
        {
            deletes_started = true;
            const std::size_t expected_next{last_delete_index.has_value() ? (*last_delete_index + 1U) : 0U};
            if (record.index < expected_next)
            {
                std::ostringstream oss;
                oss << "delete notification for file index " << record.index << " arrived out of order (after index "
                    << *last_delete_index << ")";
                return oss.str();
            }
            if ((record.index > expected_next) && (!overflow_since_last_delete))
            {
                std::ostringstream oss;
                oss << "silent loss: delete notification(s) for file index " << expected_next
                    << (record.index - 1U > expected_next ? (".." + std::to_string(record.index - 1U)) : "")
                    << " never arrived, and no IN_Q_OVERFLOW was signaled to explain the gap";
                return oss.str();
            }
            last_delete_index = record.index;
            overflow_since_last_delete = false;
        }
    }

    // Tail check: files whose create/delete was never observed at all (no later same-type event exists to
    // reveal the gap above) must still be explained by a still-pending, unconsumed IN_Q_OVERFLOW.
    if (((!last_create_index.has_value()) || (*last_create_index != (file_count - 1U))) &&
        (!overflow_since_last_create))
    {
        std::ostringstream oss;
        oss << "silent loss: create notification(s) for file index "
            << (last_create_index.has_value() ? (*last_create_index + 1U) : 0U) << ".." << (file_count - 1U)
            << " never arrived (end of cycle), and no IN_Q_OVERFLOW was signaled to explain the gap";
        return oss.str();
    }
    if (((!last_delete_index.has_value()) || (*last_delete_index != (file_count - 1U))) &&
        (!overflow_since_last_delete))
    {
        std::ostringstream oss;
        oss << "silent loss: delete notification(s) for file index "
            << (last_delete_index.has_value() ? (*last_delete_index + 1U) : 0U) << ".." << (file_count - 1U)
            << " never arrived (end of cycle), and no IN_Q_OVERFLOW was signaled to explain the gap";
        return oss.str();
    }

    return {};
}

/// \brief Verifies one cycle's bookkeeping (see AnalyzeArrivalSequence()). Logs the outcome.
///
/// \return true if every create/delete notification was either observed in order, or its absence was
///         explained by a signaled IN_Q_OVERFLOW; false if a genuine bug (reordering, phase violation, or
///         unexplained/silent loss) was detected.
bool VerifyCycle(const std::size_t cycle_index, Bookkeeping& bookkeeping)
{
    std::size_t file_count{0U};
    std::size_t overflow_count{0U};
    std::string violation{};

    {
        const std::lock_guard<std::mutex> lock{bookkeeping.mutex};
        file_count = bookkeeping.file_count;
        overflow_count = static_cast<std::size_t>(std::count_if(bookkeeping.arrival_sequence.cbegin(),
                                                                bookkeeping.arrival_sequence.cend(),
                                                                [](const ArrivalRecord& record) {
                                                                    return record.kind == ArrivalKind::kOverflow;
                                                                }));
        violation = AnalyzeArrivalSequence(bookkeeping.arrival_sequence, file_count);
    }

    if (!violation.empty())
    {
        std::cerr << "cycle " << cycle_index << ": FAILED — " << violation << " (IN_Q_OVERFLOW observed "
                  << overflow_count << " time(s) this cycle)" << std::endl;
        return false;
    }

    std::cout << "cycle " << cycle_index << ": OK — all " << file_count
              << " create/delete notifications accounted for, in order";
    if (overflow_count > 0U)
    {
        std::cout << " (IN_Q_OVERFLOW reported " << overflow_count
                  << " time(s); any resulting gaps were consistent with the expected ordering)";
    }
    std::cout << std::endl;
    return true;
}

/// \brief Runs as the forked "event producer" process for one cycle: creates \p file_count files (ascending
///        index order), then removes all of them again (ascending index order), then writes a single byte
///        to \p done_write_fd to signal completion to the parent. Never returns — always calls _exit().
[[noreturn]] void RunEventProducer(const std::string& dir_path, const std::size_t file_count, const int done_write_fd)
{
    for (std::size_t i{0U}; i < file_count; ++i)
    {
        const std::string file_path{dir_path + "/" + FileName(i)};
        const int fd{::open(file_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644)};
        if (fd < 0)
        {
            std::cerr << "producer: creating " << file_path << " failed: " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
        }
        static_cast<void>(::close(fd));
    }
    for (std::size_t i{0U}; i < file_count; ++i)
    {
        const std::string file_path{dir_path + "/" + FileName(i)};
        if ((::unlink(file_path.c_str()) != 0) && (errno != ENOENT))
        {
            std::cerr << "producer: removing " << file_path << " failed: " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
        }
    }

    const char done_byte{1};
    static_cast<void>(::write(done_write_fd, &done_byte, 1U));
    static_cast<void>(::close(done_write_fd));
    _exit(EXIT_SUCCESS);
}

/// \brief Forks an event producer for one cycle, waits for it to signal completion via the pipe (and reaps
///        it), then waits \p check_delay before returning so the caller can verify the bookkeeping.
///
/// \return true if the producer signalled completion successfully; false on any fork/pipe/producer-exit
///         failure (the bookkeeping should not be trusted in that case).
bool RunProducerCycle(const std::string& dir_path,
                      const std::size_t file_count,
                      const std::chrono::milliseconds check_delay)
{
    int pipe_fds[2]{-1, -1};
    if (::pipe(pipe_fds) != 0)
    {
        std::cerr << "pipe() failed: " << strerror(errno) << std::endl;
        return false;
    }
    const int read_fd{pipe_fds[0]};
    const int write_fd{pipe_fds[1]};

    const pid_t child_pid{::fork()};
    if (child_pid < 0)
    {
        std::cerr << "fork() failed: " << strerror(errno) << std::endl;
        static_cast<void>(::close(read_fd));
        static_cast<void>(::close(write_fd));
        return false;
    }

    if (child_pid == 0)
    {
        // Child: only ever writes to the pipe, never reads from it.
        static_cast<void>(::close(read_fd));
        RunEventProducer(dir_path, file_count, write_fd);
        // Unreachable — RunEventProducer() always calls _exit().
    }

    // Parent: only ever reads from the pipe, never writes to it.
    static_cast<void>(::close(write_fd));

    char done_byte{0};
    const ssize_t read_result{::read(read_fd, &done_byte, 1U)};
    static_cast<void>(::close(read_fd));

    int status{0};
    static_cast<void>(::waitpid(child_pid, &status, 0));

    if (read_result != 1)
    {
        std::cerr << "did not receive completion notification from event producer (pid " << child_pid << ")"
                  << std::endl;
        return false;
    }
    if (!(WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS)))
    {
        std::cerr << "event producer (pid " << child_pid << ") did not exit successfully" << std::endl;
        return false;
    }

    std::this_thread::sleep_for(check_delay);
    return true;
}

/// \brief Removes \p dir_path (best-effort; failures are logged but not fatal). Only the files this test
///        itself created are expected to have existed under it, and the producer always removes every
///        file it creates — so a plain rmdir() is enough for cleanup.
void RemoveDirectoryBestEffort(const std::string& dir_path)
{
    if ((::rmdir(dir_path.c_str()) != 0) && (errno != ENOENT))
    {
        std::cerr << "cleanup: rmdir(" << dir_path << ") failed: " << strerror(errno) << std::endl;
    }
}

}  // namespace

int main(int argc, char** argv)
{
    for (int i{1}; i < argc; ++i)
    {
        if (std::string_view{argv[i]} == "--help")
        {
            PrintUsage(argv[0]);
            return EXIT_SUCCESS;
        }
    }

    const auto maybe_config = ParseArguments(argc, argv);
    if (!maybe_config.has_value())
    {
        // A parse error; an error message and usage were already printed.
        return EXIT_FAILURE;
    }
    const Config& config{*maybe_config};

    std::cout << "inotify_event_loss: base_dir=" << config.base_dir << " file_count=" << config.file_count
              << " cycles=" << config.cycles << " check_delay_ms=" << config.check_delay.count() << std::endl;

    if ((::mkdir(config.base_dir.c_str(), 0755) != 0) && (errno != EEXIST))
    {
        std::cerr << "mkdir(" << config.base_dir << ") failed: " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    const int inotify_fd{::inotify_init()};
    if (inotify_fd < 0)
    {
        std::cerr << "inotify_init() failed: " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    const int watch_descriptor{::inotify_add_watch(inotify_fd, config.base_dir.c_str(), IN_CREATE | IN_DELETE)};
    if (watch_descriptor < 0)
    {
        std::cerr << "inotify_add_watch(" << config.base_dir << ") failed: " << strerror(errno) << std::endl;
        static_cast<void>(::close(inotify_fd));
        return EXIT_FAILURE;
    }

    Bookkeeping bookkeeping{};
    ResetBookkeeping(bookkeeping, config.file_count);

    // Detached: this thread runs for the whole lifetime of the process. It is torn down implicitly when
    // the process exits (there is nothing to join — the process is terminating either way).
    std::thread reader_thread{ReaderThreadFunc, inotify_fd, std::ref(bookkeeping)};
    reader_thread.detach();

    bool all_ok{true};
    for (std::size_t cycle{0U}; (cycle < config.cycles) && all_ok; ++cycle)
    {
        ResetBookkeeping(bookkeeping, config.file_count);

        if (!RunProducerCycle(config.base_dir, config.file_count, config.check_delay))
        {
            all_ok = false;
            break;
        }

        all_ok = VerifyCycle(cycle, bookkeeping);
    }

    static_cast<void>(::close(inotify_fd));
    RemoveDirectoryBestEffort(config.base_dir);

    if (!all_ok)
    {
        std::cerr << "inotify_event_loss: FAILED" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "inotify_event_loss: all " << config.cycles << " cycles passed" << std::endl;
    return EXIT_SUCCESS;
}
