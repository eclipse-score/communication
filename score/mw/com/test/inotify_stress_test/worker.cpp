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

#include "score/mw/com/test/inotify_stress_test/worker.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/inotify_stress_test/inotify_stress_test_internal.h"
#include "score/os/inotify.h"
#include "score/os/utils/inotify/inotify_event.h"
#include "score/os/utils/inotify/inotify_instance_impl.h"

#include <score/stop_token.hpp>
#include <cstdint>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <charconv>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace score::mw::com::test
{

namespace
{

constexpr mode_t kExpectedDirMode{0755U};

/// \brief Maximum number of add+remove attempts per cycle before a worker reports failure. Tolerates the
///        transient EINVAL the QNX io-notify manager can return under heavy concurrent watch churn.
constexpr std::size_t kMaxWatchAttempts{5U};

/// \brief Waits for the controller's next signal. Logs and returns false (worker shall exit) if it is
///        anything other than PROCEED_NEXT_CHECKPOINT (i.e. FINISH_ACTIONS or an aborted wait).
bool WaitForProceedOrExit(const pid_t pid,
                          const std::size_t cycle_index,
                          CheckPointControl& checkpoint_control,
                          const score::cpp::stop_token& stop_token)
{
    const auto instruction = WaitForChildProceed(checkpoint_control, stop_token);
    if (instruction != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
    {
        std::cerr << pid << ": Received non-proceed instruction at cycle " << cycle_index << ", exiting." << std::endl;
        return false;
    }
    return true;
}

/// \brief Returns true when \p path exists and its lower nine permission bits match \p expected_mode.
bool HasCorrectPermissions(const pid_t pid, const std::string& path, const mode_t expected_mode)
{
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0)
    {
        std::cerr << pid << ": stat on " << path << " failed: " << strerror(errno) << std::endl;
        return false;
    }
    if ((st.st_mode & 0777U) != expected_mode)
    {
        std::cerr << pid << ": Directory " << path << " has wrong permissions: actual " << std::oct
                  << (st.st_mode & 0777U) << ", expected " << expected_mode << std::dec << std::endl;
        return false;
    }
    return true;
}

/// \brief Ensures the shared directory exists with the expected mode. Creates it when absent, otherwise
///        verifies its permissions. Returns false (after logging) on any failure.
bool EnsureDirectory(const pid_t pid, const std::string& test_dir)
{
    const int result{::mkdir(test_dir.c_str(), kExpectedDirMode)};
    if (result == 0)
    {
        // This process created the directory and owns it — safe to chmod.
        if (::chmod(test_dir.c_str(), kExpectedDirMode) != 0)
        {
            std::cerr << pid << ": chmod on " << test_dir << " failed: " << strerror(errno) << std::endl;
            return false;
        }
        std::cerr << pid << ": Created directory: " << test_dir << std::endl;
        return true;
    }
    if (errno == EEXIST)
    {
        // Another worker already created it — only verify permissions, never chmod.
        // HasCorrectPermissions logs the specific reason (stat failure or mode mismatch) on failure.
        if (!HasCorrectPermissions(pid, test_dir, kExpectedDirMode))
        {
            return false;
        }
        std::cerr << pid << ": Directory already exists, skipping: " << test_dir << std::endl;
        return true;
    }
    std::cerr << pid << ": mkdir " << test_dir << " failed: " << strerror(errno) << std::endl;
    return false;
}

/// \brief Runs one add-watch/remove-watch churn cycle (TestMode::kWatchChurn).
///
/// \return true if the cycle succeeded; false if an unrecoverable error was reported to
///         \p checkpoint_control and the worker shall exit.
bool RunWatchChurnCycle(const pid_t pid,
                        const std::string& test_dir,
                        os::InotifyInstanceImpl& inotify,
                        CheckPointControl& checkpoint_control)
{
    // Step 1: Ensure the shared process directory exists with the expected permissions
    if (!EnsureDirectory(pid, test_dir))
    {
        checkpoint_control.ErrorOccurred();
        return false;
    }

    // Steps 2 & 3: Add an inotify watch on the shared base folder and remove it immediately.
    // Under heavy concurrent add/remove churn on the same directory, the QNX io-notify resource
    // manager can transiently fail a removal with EINVAL. Once that happens the watch descriptor is
    // permanently invalid — retrying RemoveWatch on the same descriptor keeps returning EINVAL — so
    // recovery requires obtaining a fresh descriptor via AddWatch. Therefore the whole add+remove
    // step is retried a bounded number of times, and the worker only fails if it cannot complete a
    // clean add+remove within kMaxWatchAttempts. Any non-EINVAL error is a genuine failure.
    bool watch_cycle_succeeded{false};
    for (std::size_t attempt{0U}; (attempt < kMaxWatchAttempts) && (!watch_cycle_succeeded); ++attempt)
    {
        auto watch_descriptor =
            inotify.AddWatch(kBaseFolder, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete);
        if (!watch_descriptor.has_value())
        {
            std::cerr << pid << ": AddWatch failed: " << watch_descriptor.error() << " on " << kBaseFolder << std::endl;
            checkpoint_control.ErrorOccurred();
            return false;
        }

        const auto remove_result = inotify.RemoveWatch(watch_descriptor.value());
        if (remove_result.has_value())
        {
            watch_cycle_succeeded = true;
        }
        else if (remove_result.error().GetOsDependentErrorCode() != EINVAL)
        {
            std::cerr << pid << ": RemoveWatch failed: " << remove_result.error() << std::endl;
            checkpoint_control.ErrorOccurred();
            return false;
        }
        else
        {
            // Transient EINVAL: the descriptor is now invalid, so the next attempt re-adds the watch
            // to obtain a fresh descriptor before removing again.
            std::cerr << pid << ": RemoveWatch transient EINVAL (attempt " << (attempt + 1U) << " of "
                      << kMaxWatchAttempts << "), re-adding watch and retrying" << std::endl;
        }
    }

    if (!watch_cycle_succeeded)
    {
        std::cerr << pid << ": RemoveWatch failed persistently after " << kMaxWatchAttempts << " attempts" << std::endl;
        checkpoint_control.ErrorOccurred();
        return false;
    }

    return true;
}

/// \brief Runs the TestMode::kWatchChurn worker loop.
void RunWatchChurnWorker(const pid_t pid,
                         const std::size_t cycles,
                         const std::string& test_dir,
                         os::InotifyInstanceImpl& inotify,
                         CheckPointControl& checkpoint_control,
                         const score::cpp::stop_token& stop_token)
{
    for (std::size_t i{0U}; i < cycles; ++i)
    {
        if (!WaitForProceedOrExit(pid, i, checkpoint_control, stop_token))
        {
            return;
        }
        if (!RunWatchChurnCycle(pid, test_dir, inotify, checkpoint_control))
        {
            return;
        }
        checkpoint_control.CheckPointReached(kCycleDoneCheckpoint);
    }

    // Wait for controller's final FinishActions signal before exiting
    static_cast<void>(WaitForChildProceed(checkpoint_control, stop_token));
}

/// \brief Blocks on \p inotify.Read() (via a helper thread) until either an event arrives or \p timeout
///        elapses. On timeout, closes \p inotify to unblock the pending read (the instance becomes unusable
///        afterwards — appropriate here since a timeout is treated as a fatal error for the worker).
///
/// \return The events read, or std::nullopt on timeout (already logged as N/A here — caller logs specifics).
std::optional<score::cpp::static_vector<os::InotifyEvent, os::InotifyInstanceImpl::max_events>> ReadWithTimeout(
    os::InotifyInstanceImpl& inotify,
    const std::chrono::milliseconds timeout)
{
    auto read_future = std::async(std::launch::async, [&inotify]() {
        return inotify.Read();
    });

    if (read_future.wait_for(timeout) == std::future_status::timeout)
    {
        // Unblocks the pending Read() call in the helper thread.
        inotify.Close();
        static_cast<void>(read_future.wait());
        return std::nullopt;
    }

    auto read_result = read_future.get();
    if (!read_result.has_value())
    {
        std::cerr << "Read() failed: " << read_result.error() << std::endl;
        return std::nullopt;
    }
    return read_result.value();
}

/// \brief Waits for the inotify notification expected in cycle \p cycle_index (create on even cycles, delete
///        on odd cycles) for kNotifyTestFileName within \p notify_timeout of being called.
///
/// \return true if the expected notification arrived in time; false if it timed out, an OS error occurred, or
///         an unexpected event was observed. In all false cases an error has already been logged and reported
///         to \p checkpoint_control via ErrorOccurred().
bool WaitForExpectedNotification(const pid_t pid,
                                 const std::size_t cycle_index,
                                 os::InotifyInstanceImpl& inotify,
                                 const std::chrono::milliseconds notify_timeout,
                                 CheckPointControl& checkpoint_control)
{
    const bool expect_create{(cycle_index % 2U) == 0U};
    const auto expected_mask =
        expect_create ? os::InotifyEvent::ReadMask::kInCreate : os::InotifyEvent::ReadMask::kInDelete;
    const char* const expected_action{expect_create ? "create" : "delete"};

    const auto read_result = ReadWithTimeout(inotify, notify_timeout);
    if (!read_result.has_value())
    {
        std::cerr << pid << ": cycle " << cycle_index << ": did not observe expected '" << expected_action
                  << "' notification for " << kNotifyTestFileName << " within " << notify_timeout.count() << "ms"
                  << std::endl;
        checkpoint_control.ErrorOccurred();
        return false;
    }

    for (const auto& event : read_result.value())
    {
        if ((event.GetMask() & expected_mask) && (event.GetName() == kNotifyTestFileName))
        {
            return true;
        }
    }

    std::cerr << pid << ": cycle " << cycle_index << ": received " << read_result.value().size()
              << " event(s), none matching expected '" << expected_action << "' notification for "
              << kNotifyTestFileName << std::endl;
    checkpoint_control.ErrorOccurred();
    return false;
}

/// \brief Runs the TestMode::kNotifyLatency worker loop. Establishes the watch once up front — this mode
///        exercises notification latency, not watch add/remove churn — and keeps it for the whole run so
///        that events queued by the controller's file operations are never missed between cycles.
void RunNotifyLatencyWorker(const pid_t pid,
                            const std::size_t cycles,
                            os::InotifyInstanceImpl& inotify,
                            const std::chrono::milliseconds notify_timeout,
                            CheckPointControl& checkpoint_control,
                            const score::cpp::stop_token& stop_token)
{
    auto watch_descriptor =
        inotify.AddWatch(kBaseFolder, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete);
    if (!watch_descriptor.has_value())
    {
        std::cerr << pid << ": AddWatch failed: " << watch_descriptor.error() << " on " << kBaseFolder << std::endl;
        checkpoint_control.ErrorOccurred();
        return;
    }
    // Tell the controller the watch is armed before it performs any file operation — otherwise the
    // controller could act before this worker's watch exists and the notification would be missed.
    checkpoint_control.CheckPointReached(kWatchReadyCheckpoint);

    for (std::size_t i{0U}; i < cycles; ++i)
    {
        // The controller's signal also announces that it is about to create/remove the shared notify
        // test file, so the notify_timeout clock effectively starts here.
        if (!WaitForProceedOrExit(pid, i, checkpoint_control, stop_token))
        {
            return;
        }
        if (!WaitForExpectedNotification(pid, i, inotify, notify_timeout, checkpoint_control))
        {
            return;
        }
        checkpoint_control.CheckPointReached(kCycleDoneCheckpoint);
    }

    // Wait for controller's final FinishActions signal before exiting
    static_cast<void>(WaitForChildProceed(checkpoint_control, stop_token));
}

/// \brief Distinguishes what kind of event an ArrivalRecord represents: a create or delete notification for
///        a burst file, or the kernel's own IN_Q_OVERFLOW indicator (queue overflowed, events were dropped
///        around this point). \c index is meaningless for kOverflow.
enum class ArrivalKind : std::uint8_t
{
    kCreate,
    kDelete,
    kOverflow,
};

/// \brief Per-file create/delete observation bookkeeping for TestMode::kBurstLoss, shared between the main
///        thread (which resets it at the start of a cycle and inspects it at the end) and the background
///        reader thread (which continuously updates it as events arrive).
struct BurstBookkeeping
{
    /// \brief One entry per create/delete/overflow event observed, in the exact order the reader thread
    ///        drained them from inotify — used to detect both loss and reordering (see
    ///        AnalyzeArrivalSequence).
    struct ArrivalRecord
    {
        ArrivalKind kind;
        std::size_t index;
    };

    std::mutex mutex{};
    std::size_t file_count{0U};
    /// Every create/delete/overflow event observed this cycle, in arrival order. The controller creates all
    /// burst_file_count files (in ascending index order), then removes them all (again in ascending index
    /// order), so a healthy run's arrival_sequence — with any IN_Q_OVERFLOW markers removed — must read:
    /// create_0, create_1, ..., create_{N-1}, delete_0, delete_1, ..., delete_{N-1} (see
    /// AnalyzeArrivalSequence for how gaps interspersed with IN_Q_OVERFLOW markers are handled).
    std::vector<ArrivalRecord> arrival_sequence{};
};

/// \brief Parses the burst-file index out of an event's file name (as produced by BurstFileName()).
///        Returns std::nullopt if \p name doesn't match the expected "<prefix><digits>" pattern.
std::optional<std::size_t> ParseBurstFileIndex(const std::string_view name)
{
    if (name.size() <= kBurstFilePrefix.size())
    {
        return std::nullopt;
    }
    if (name.substr(0U, kBurstFilePrefix.size()) != kBurstFilePrefix)
    {
        return std::nullopt;
    }
    const auto digits = name.substr(kBurstFilePrefix.size());
    std::size_t value{0U};
    const auto parse_result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if ((parse_result.ec != std::errc{}) || (parse_result.ptr != (digits.data() + digits.size())))
    {
        return std::nullopt;
    }
    return value;
}

/// \brief Continuously drains inotify events into \p bookkeeping until \p inotify is closed (worker
///        shutdown) or Read() otherwise fails. Runs for the entire lifetime of the worker (not just during
///        a cycle) so a burst of events arriving faster than the main thread cycles through checkpoints is
///        never missed — inotify.Read() blocks until at least one event is available and returns up to
///        max_events per call, so this loop is the continuous drain a real, healthy consumer would run.
void BurstReaderThreadFunc(os::InotifyInstanceImpl& inotify, BurstBookkeeping& bookkeeping)
{
    while (true)
    {
        auto read_result = inotify.Read();
        if (!read_result.has_value())
        {
            // inotify.Close() (worker shutdown) or a genuine OS error — either way, done.
            return;
        }

        const std::lock_guard<std::mutex> lock{bookkeeping.mutex};
        for (const auto& event : read_result.value())
        {
            if ((event.GetMask() & os::InotifyEvent::ReadMask::kInQOverflow))
            {
                bookkeeping.arrival_sequence.push_back({ArrivalKind::kOverflow, 0U});
                std::cerr << "inotify reported IN_Q_OVERFLOW — the kernel's event queue overflowed and "
                             "dropped events"
                          << std::endl;
            }

            const auto index = ParseBurstFileIndex(event.GetName());
            if ((!index.has_value()) || (*index >= bookkeeping.file_count))
            {
                continue;
            }
            if ((event.GetMask() & os::InotifyEvent::ReadMask::kInCreate))
            {
                bookkeeping.arrival_sequence.push_back({ArrivalKind::kCreate, *index});
            }
            if ((event.GetMask() & os::InotifyEvent::ReadMask::kInDelete))
            {
                bookkeeping.arrival_sequence.push_back({ArrivalKind::kDelete, *index});
            }
        }
    }
}

/// \brief Resets \p bookkeeping for a fresh cycle of \p file_count files.
void ResetBurstBookkeeping(BurstBookkeeping& bookkeeping, const std::size_t file_count)
{
    const std::lock_guard<std::mutex> lock{bookkeeping.mutex};
    bookkeeping.file_count = file_count;
    bookkeeping.arrival_sequence.clear();
    bookkeeping.arrival_sequence.reserve(2U * file_count);
}

/// \brief Analyzes \p arrival_sequence (a consistent snapshot, taken under the bookkeeping mutex) for
///        violations of the expected delivery order and for *unexplained* (silent) notification loss.
///
/// Background: it is not, by itself, a bug for the watcher to miss create/delete notifications under heavy
/// load — as long as the kernel's IN_Q_OVERFLOW indicator correctly signals that its event queue overflowed
/// and events were dropped at that point. What IS a bug is either (a) notifications arriving out of order,
/// or (b) notifications going missing WITHOUT IN_Q_OVERFLOW ever being raised to explain it.
///
/// Since the controller creates all file_count files (in ascending index order), then removes all of them
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
std::string AnalyzeArrivalSequence(const std::vector<BurstBookkeeping::ArrivalRecord>& arrival_sequence,
                                   const std::size_t file_count)
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

/// \brief Verifies one cycle's bookkeeping (see AnalyzeArrivalSequence()). Logs the outcome and, on
///        failure, reports ErrorOccurred to \p checkpoint_control.
///
/// \return true if every create/delete notification was either observed in order, or its absence was
///         explained by a signaled IN_Q_OVERFLOW; false if a genuine bug (reordering, phase violation, or
///         unexplained/silent loss) was detected.
bool VerifyBurstCycle(const pid_t pid,
                      const std::size_t cycle_index,
                      BurstBookkeeping& bookkeeping,
                      CheckPointControl& checkpoint_control)
{
    std::size_t file_count{0U};
    std::size_t overflow_count{0U};
    std::string violation{};

    {
        const std::lock_guard<std::mutex> lock{bookkeeping.mutex};
        file_count = bookkeeping.file_count;
        overflow_count = static_cast<std::size_t>(std::count_if(bookkeeping.arrival_sequence.cbegin(),
                                                                bookkeeping.arrival_sequence.cend(),
                                                                [](const BurstBookkeeping::ArrivalRecord& record) {
                                                                    return record.kind == ArrivalKind::kOverflow;
                                                                }));
        violation = AnalyzeArrivalSequence(bookkeeping.arrival_sequence, file_count);
    }

    if (!violation.empty())
    {
        std::cerr << pid << ": cycle " << cycle_index << ": FAILED — " << violation << " (IN_Q_OVERFLOW observed "
                  << overflow_count << " time(s) this cycle)" << std::endl;
        checkpoint_control.ErrorOccurred();
        return false;
    }

    std::cout << pid << ": cycle " << cycle_index << ": OK — all " << file_count
              << " create/delete notifications accounted for, in order";
    if (overflow_count > 0U)
    {
        std::cout << " (IN_Q_OVERFLOW reported " << overflow_count
                  << " time(s); any resulting gaps were consistent with the expected ordering)";
    }
    std::cout << std::endl;
    return true;
}

/// \brief Runs the TestMode::kBurstLoss worker loop. Establishes the watch once up front and runs a
///        background thread that continuously drains events for the whole lifetime of the worker (see
///        BurstReaderThreadFunc), independent of the cycle checkpoints below.
void RunBurstLossWorker(const pid_t pid,
                        const std::size_t cycles,
                        os::InotifyInstanceImpl& inotify,
                        const std::size_t burst_file_count,
                        const std::chrono::milliseconds burst_check_delay,
                        CheckPointControl& checkpoint_control,
                        const score::cpp::stop_token& stop_token)
{
    auto watch_descriptor =
        inotify.AddWatch(kBaseFolder, os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete);
    if (!watch_descriptor.has_value())
    {
        std::cerr << pid << ": AddWatch failed: " << watch_descriptor.error() << " on " << kBaseFolder << std::endl;
        checkpoint_control.ErrorOccurred();
        return;
    }

    BurstBookkeeping bookkeeping{};
    bookkeeping.file_count = burst_file_count;
    std::thread reader_thread{BurstReaderThreadFunc, std::ref(inotify), std::ref(bookkeeping)};

    // Tell the controller the watch is armed (and the reader thread running) before it creates any burst
    // file — otherwise the controller could act before this worker is ready to observe it.
    checkpoint_control.CheckPointReached(kWatchReadyCheckpoint);

    bool aborted{false};
    for (std::size_t i{0U}; (i < cycles) && (!aborted); ++i)
    {
        // 1. Controller announces cycle i is starting — reset bookkeeping and confirm we're ready before
        //    it creates a single file, to avoid racing the reset against the first event.
        if (!WaitForProceedOrExit(pid, i, checkpoint_control, stop_token))
        {
            aborted = true;
            break;
        }
        ResetBurstBookkeeping(bookkeeping, burst_file_count);
        checkpoint_control.CheckPointReached(kCycleReadyCheckpoint);

        // 2. Controller announces cycle i has ended (it created, then removed, all burst_file_count files).
        if (!WaitForProceedOrExit(pid, i, checkpoint_control, stop_token))
        {
            aborted = true;
            break;
        }

        // 3. Grace period for any still in-flight notifications to be dispatched and drained.
        std::this_thread::sleep_for(burst_check_delay);

        if (VerifyBurstCycle(pid, i, bookkeeping, checkpoint_control))
        {
            checkpoint_control.CheckPointReached(kCycleDoneCheckpoint);
        }
        else
        {
            aborted = true;
        }
    }

    if (!aborted)
    {
        // Wait for controller's final FinishActions signal before exiting
        static_cast<void>(WaitForChildProceed(checkpoint_control, stop_token));
    }

    // Unblocks the reader thread's pending Read() so it can be joined before this function returns — a
    // joinable std::thread whose destructor runs while still joinable calls std::terminate().
    inotify.Close();
    reader_thread.join();
}

}  // namespace

void RunWorkerProcess(const std::size_t worker_index,
                      const std::size_t cycles,
                      CheckPointControl& checkpoint_control,
                      const StressTestConfig& config)
{
    const auto pid{getpid()};
    const std::string test_dir{TestDir()};
    static_cast<void>(worker_index);

    const score::cpp::stop_source worker_stop_source{};
    os::InotifyInstanceImpl inotify{};

    switch (config.mode)
    {
        case TestMode::kWatchChurn:
            RunWatchChurnWorker(pid, cycles, test_dir, inotify, checkpoint_control, worker_stop_source.get_token());
            break;
        case TestMode::kNotifyLatency:
            RunNotifyLatencyWorker(
                pid, cycles, inotify, config.notify_timeout, checkpoint_control, worker_stop_source.get_token());
            break;
        case TestMode::kBurstLoss:
            RunBurstLossWorker(pid,
                               cycles,
                               inotify,
                               config.burst_file_count,
                               config.burst_check_delay,
                               checkpoint_control,
                               worker_stop_source.get_token());
            break;
    }
}

bool SetWorkerCredentials(const std::string& worker_name,
                          const std::uint32_t base_gid,
                          const std::uint32_t base_uid,
                          const std::size_t worker_index)
{
    if (base_gid != 0U)
    {
        if (::setgid(static_cast<gid_t>(base_gid + static_cast<std::uint32_t>(worker_index))) != 0)
        {
            std::cerr << worker_name << ": setgid failed: " << strerror(errno) << std::endl;
            return false;
        }
    }
    if (base_uid != 0U)
    {
        if (::setuid(static_cast<uid_t>(base_uid + static_cast<std::uint32_t>(worker_index))) != 0)
        {
            std::cerr << worker_name << ": setuid failed: " << strerror(errno) << std::endl;
            return false;
        }
    }
    return true;
}

}  // namespace score::mw::com::test
