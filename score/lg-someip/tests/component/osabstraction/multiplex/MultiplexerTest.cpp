/********************************************************************************
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
 *
 ********************************************************************************/

// The Multiplexer contract in multiplex/Multiplexer.h, driven by the multiplexer's own loop thread.
//
// A case that needs the loop held makes it block inside a callback on a LoopGate the test thread keeps closed.
// Calls made meanwhile run on an OffThreadCall, so a call that blocks fails the case instead of hanging it.
// An unset() from a callback that waits for that callback deadlocks the loop, and that case hangs until the timeout.

#include "test_utils.h"

#include <multiplex/Multiplexer.h>
#include <utils/time/TimerManager.h>

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using lgsomeip::osabstraction::Multiplexer;
using test_utils::Clock;
using test_utils::DrainByte;
using test_utils::PokeByte;
using test_utils::SocketPair;
using test_utils::UniqueFd;
using test_utils::WaitFor;

namespace
{

// Generous, so a loaded host fails a case only when the event never comes.
constexpr std::chrono::seconds kEventTimeout{5};

// How long a case watches for something that must not happen.
constexpr std::chrono::milliseconds kQuietWindow{300};

// Registrations a Multiplexer accepts before it reports a full reactor.
constexpr std::size_t kReactorCapacity{256U};

// Marks a recorded count that was never written.
constexpr std::uint32_t kNotRecorded{std::numeric_limits<std::uint32_t>::max()};

Clock::time_point event_deadline()
{
    return Clock::now() + kEventTimeout;
}

// Waits until counter reaches n.
bool reaches(const std::atomic<std::uint32_t>& counter, const std::uint32_t n)
{
    return WaitFor(
        [&counter, n]() {
            return counter.load() >= n;
        },
        event_deadline());
}

// A callback that drains one byte from fd per call and counts the calls.
std::function<void(bool)> counting(const int fd, std::atomic<std::uint32_t>& calls)
{
    return [fd, &calls](bool) {
        DrainByte(fd);
        ++calls;
    };
}

// Records how often and on which threads a callback ran.
class CallLog final
{
  public:
    void record()
    {
        std::lock_guard<std::mutex> lock{mutex_};
        threads_.push_back(std::this_thread::get_id());
    }

    std::size_t count() const
    {
        std::lock_guard<std::mutex> lock{mutex_};
        return threads_.size();
    }

    std::vector<std::thread::id> threads() const
    {
        std::lock_guard<std::mutex> lock{mutex_};
        return threads_;
    }

  private:
    mutable std::mutex mutex_{};
    std::vector<std::thread::id> threads_{};
};

// A socket pair whose callback drains one byte per call and counts the calls.
// Cases use it to see the loop complete passes without sleeping.
// It outlives every Multiplexer its callback is set on.
class Probe final
{
  public:
    bool open()
    {
        return pair_.Open();
    }

    int fd() const
    {
        return pair_.ReadFd();
    }

    std::function<void(bool)> callback()
    {
        return counting(pair_.ReadFd(), calls_);
    }

    // Pokes the descriptor and waits for its callback.
    // Returns false when the callback does not run within kEventTimeout.
    // The loop runs one callback at a time, so a callback it started before the probe's has returned by then.
    bool completes_pass()
    {
        const std::uint32_t before{calls_.load()};
        PokeByte(pair_.WriteFd());
        return reaches(calls_, before + 1U);
    }

  private:
    SocketPair pair_{};
    std::atomic<std::uint32_t> calls_{0U};
};

// Holds the loop thread inside a callback that calls pass() while the test thread keeps the gate closed.
// A case declares the gate before the Multiplexer and the lock from close() after it.
// A failed assertion then releases the lock before the destructor joins the loop.
class LoopGate final
{
  public:
    // Runs on the loop thread inside a callback.
    // It reports the arrival and waits while the gate is closed.
    void pass()
    {
        arrived_ = true;
        const std::lock_guard<std::mutex> wait{mutex_};
    }

    // The next pass() blocks until the returned lock is released.
    std::unique_lock<std::mutex> close()
    {
        arrived_ = false;
        return std::unique_lock<std::mutex>{mutex_};
    }

    // Waits until the loop thread calls pass().
    bool reached() const
    {
        return WaitFor(
            [this]() {
                return arrived_.load();
            },
            event_deadline());
    }

  private:
    std::mutex mutex_{};
    std::atomic_bool arrived_{false};
};

// Runs one call on a thread of its own, so a call that deadlocks fails the case instead of hanging it.
// The destructor joins, so whatever the call may wait on must be released before the object goes out of scope.
class OffThreadCall final
{
  public:
    explicit OffThreadCall(std::function<void()> call)
        : thread_{[this, call]() {
              call();
              done_ = true;
          }}
    {
    }

    ~OffThreadCall()
    {
        thread_.join();
    }

    OffThreadCall(const OffThreadCall&) = delete;
    OffThreadCall& operator=(const OffThreadCall&) = delete;
    OffThreadCall(OffThreadCall&&) = delete;
    OffThreadCall& operator=(OffThreadCall&&) = delete;

    bool returns_by(const Clock::time_point deadline)
    {
        return WaitFor(
            [this]() {
                return done_.load();
            },
            deadline);
    }

  private:
    std::atomic_bool done_{false};
    std::thread thread_;
};

// Puts the read end of pair on descriptor number fd, as a socket opened right after fd was closed gets it.
// fd must be closed, so no other descriptor of the case is replaced.
bool move_read_end_to(SocketPair& pair, const int fd)
{
    if (::dup2(pair.ReadFd(), fd) != fd)
    {
        return false;
    }
    pair.ReadEnd() = UniqueFd{fd};
    return true;
}

// Moves the read end of new_pair onto the read fd number of old_pair, as a replaced endpoint reuses an fd number.
// The sequence is unset(), close, a new socket on the same number, and set() with callback.
// Returns false when the fd number could not be moved.
// The set() runs in either case.
bool reuse_fd(Multiplexer& mux, SocketPair& old_pair, SocketPair& new_pair, std::function<void(bool)> callback)
{
    const int fd{old_pair.ReadFd()};
    mux.unset(fd);
    old_pair.ReadEnd().Close();
    const bool moved{move_read_end_to(new_pair, fd)};
    mux.set(fd, std::move(callback));
    return moved;
}

// Redirects std::cout, where lgsomeip::Logger writes its console records, for as long as the object lives.
class CoutCapture final
{
  public:
    CoutCapture() : original_buffer_{std::cout.rdbuf(captured_.rdbuf())} {}

    ~CoutCapture()
    {
        std::cout.rdbuf(original_buffer_);
    }

    CoutCapture(const CoutCapture&) = delete;
    CoutCapture& operator=(const CoutCapture&) = delete;

    std::string str() const
    {
        return captured_.str();
    }

  private:
    std::ostringstream captured_{};
    std::streambuf* original_buffer_;
};

// Sets the soft RLIMIT_NOFILE and restores the original limit on destruction.
class FdLimit final
{
  public:
    FdLimit()
    {
        valid_ = (::getrlimit(RLIMIT_NOFILE, &original_) == 0);
    }

    ~FdLimit()
    {
        if (valid_)
        {
            static_cast<void>(::setrlimit(RLIMIT_NOFILE, &original_));
        }
    }

    FdLimit(const FdLimit&) = delete;
    FdLimit& operator=(const FdLimit&) = delete;

    // Raises the soft limit to at least `wanted` when the hard limit allows it.
    // Returns whether the soft limit is at least `wanted` afterwards.
    bool ensure_at_least(const rlim_t wanted)
    {
        if (!valid_)
        {
            return false;
        }
        if (original_.rlim_cur == RLIM_INFINITY || original_.rlim_cur >= wanted)
        {
            return true;
        }
        if (original_.rlim_max != RLIM_INFINITY && original_.rlim_max < wanted)
        {
            return false;
        }
        rlimit raised{original_};
        raised.rlim_cur = wanted;
        return ::setrlimit(RLIMIT_NOFILE, &raised) == 0;
    }

    // Lowers the soft limit to `limit`, so no descriptor numbered `limit` or higher can be opened.
    bool set_soft(const rlim_t limit)
    {
        if (!valid_)
        {
            return false;
        }
        rlimit lowered{original_};
        lowered.rlim_cur = limit;
        return ::setrlimit(RLIMIT_NOFILE, &lowered) == 0;
    }

  private:
    rlimit original_{};
    bool valid_{false};
};

}  // namespace

// [Multiplexer::set]: `callback` runs on the multiplexer thread and receives `false` for data.
TEST(Multiplexer, calls_callbacks_with_false_on_one_thread_other_than_the_caller)
{
    SocketPair first{};
    SocketPair second{};
    ASSERT_TRUE(first.Open());
    ASSERT_TRUE(second.Open());
    const int first_fd{first.ReadFd()};
    const int second_fd{second.ReadFd()};
    CallLog log{};
    std::atomic_bool got_true{false};

    Multiplexer mux{};
    mux.start();
    mux.set(first_fd, [&](const bool arg) {
        DrainByte(first_fd);
        if (arg)
        {
            got_true = true;
        }
        log.record();
    });
    mux.set(second_fd, [&](const bool arg) {
        DrainByte(second_fd);
        if (arg)
        {
            got_true = true;
        }
        log.record();
    });

    PokeByte(first.WriteFd());
    PokeByte(second.WriteFd());
    ASSERT_TRUE(WaitFor(
        [&log]() {
            return log.count() >= 2U;
        },
        event_deadline()))
        << "a readable fd was not dispatched";

    EXPECT_FALSE(got_true.load()) << "a callback was called with true";
    const std::vector<std::thread::id> threads{log.threads()};
    for (const std::thread::id& thread : threads)
    {
        EXPECT_NE(thread, std::this_thread::get_id()) << "a callback ran on the thread that called set()";
        EXPECT_EQ(thread, threads.front()) << "callbacks ran on more than one thread";
    }
}

// [Multiplexer::set_timeout]: `callback` runs on the multiplexer thread when a pass ends after an idle second.
TEST(Multiplexer, calls_the_timeout_callback_after_an_idle_second)
{
    CallLog log{};

    Multiplexer mux{};
    mux.set_timeout([&log]() {
        log.record();
    });
    const Clock::time_point started{Clock::now()};
    mux.start();

    ASSERT_TRUE(WaitFor(
        [&log]() {
            return log.count() >= 1U;
        },
        event_deadline()))
        << "the timeout callback never ran";
    const auto idle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();

    // The loop waits 1000 ms per pass, so a much earlier call means something other than a timeout ran the callback.
    EXPECT_GE(idle_ms, 500);
    EXPECT_NE(log.threads().front(), std::this_thread::get_id());
}

// [Multiplexer::set_timeout]: a pass that a readable fd ends is not idle, so it does not run the timeout callback.
TEST(Multiplexer, does_not_call_the_timeout_callback_on_a_wake_by_data)
{
    Probe probe{};
    ASSERT_TRUE(probe.open());
    std::atomic<std::uint32_t> timeouts{0U};

    Multiplexer mux{};
    mux.set_timeout([&timeouts]() {
        ++timeouts;
    });
    mux.start();
    mux.set(probe.fd(), probe.callback());

    // Wakes spaced well below the idle second, over more than a second in total.
    // A loop that calls the timeout callback once a second regardless of the wakes fails here.
    constexpr std::chrono::milliseconds kSpacing{300};
    constexpr int kRounds{5};
    Clock::time_point last_wake{Clock::now()};
    Clock::duration longest_gap{};
    for (int round{0}; round < kRounds; ++round)
    {
        std::this_thread::sleep_for(kSpacing);
        ASSERT_TRUE(probe.completes_pass()) << "round " << round;
        const Clock::time_point now{Clock::now()};
        longest_gap = std::max(longest_gap, now - last_wake);
        last_wake = now;
    }

    if (longest_gap >= std::chrono::milliseconds{900})
    {
        GTEST_SKIP() << "the host stalled for "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(longest_gap).count()
                     << " ms between two wakes, so a genuine idle second may have passed";
    }
    EXPECT_EQ(timeouts.load(), 0U) << "the timeout callback ran although no pass was idle for a second";
}

#if defined(LINUX)
// [Multiplexer]: on Linux, every pass also runs TimerManager::iterate_loop().
TEST(Multiplexer, fires_a_timer_manager_one_shot_timer_while_running)
{
    lgsomeip::TimerManager& timers{lgsomeip::TimerManager::get()};
    // Shared, so a timer that fires after the case ends still writes to live memory.
    const auto log = std::make_shared<CallLog>();

    Multiplexer mux{};
    mux.start();
    const lgsomeip::TimerManager::TimerId id{
        timers.start_one_shot_timer(std::chrono::milliseconds{50}, [log](lgsomeip::TimerManager::TimerId) {
            log->record();
        })};

    const bool fired{WaitFor(
        [&log]() {
            return log->count() >= 1U;
        },
        event_deadline())};
    // The singleton keeps a fired one-shot timer listed, so the case removes it to leave nothing for the next one.
    timers.kill_timer(id);

    ASSERT_TRUE(fired) << "the loop does not drive TimerManager";
    EXPECT_NE(log->threads().front(), std::this_thread::get_id());
}
#endif  // LINUX

// [Multiplexer::unset]: called from another thread, it waits until a running callback of the fd returns.
// No callback of the fd runs after it returns.
TEST(Multiplexer, off_thread_unset_waits_for_the_running_callback_and_stops_its_dispatch)
{
    SocketPair pair{};
    Probe probe{};
    ASSERT_TRUE(pair.Open());
    ASSERT_TRUE(probe.open());
    const int fd{pair.ReadFd()};
    LoopGate gate{};
    std::atomic<std::uint32_t> calls{0U};
    std::atomic_bool completed{false};

    Multiplexer mux{};
    mux.start();
    mux.set(probe.fd(), probe.callback());
    mux.set(fd, [&](bool) {
        DrainByte(fd);
        ++calls;
        gate.pass();
        completed = true;
    });

    std::unique_lock<std::mutex> hold{gate.close()};
    PokeByte(pair.WriteFd());
    ASSERT_TRUE(gate.reached()) << "the callback never ran";

    bool returned_while_callback_ran{false};
    bool unset_returned{false};
    std::atomic_bool completed_at_return{false};
    {
        OffThreadCall call{[&]() {
            mux.unset(fd);
            completed_at_return = completed.load();
        }};
        returned_while_callback_ran = call.returns_by(Clock::now() + kQuietWindow);
        hold.unlock();
        unset_returned = call.returns_by(event_deadline());
    }
    ASSERT_TRUE(unset_returned) << "unset() did not return after the callback completed";
    EXPECT_FALSE(returned_while_callback_ran) << "unset() returned while the fd's callback ran";
    EXPECT_TRUE(completed_at_return.load()) << "unset() returned before the fd's callback completed";

    const std::uint32_t calls_at_unset{calls.load()};
    PokeByte(pair.WriteFd());
    ASSERT_TRUE(probe.completes_pass());
    ASSERT_TRUE(probe.completes_pass());
    EXPECT_EQ(calls.load(), calls_at_unset) << "a callback ran after unset() returned";
}

// [Multiplexer::unset]: no callback of the fd runs after this returns, also one the loop picked for the running batch.
TEST(Multiplexer, no_callback_starts_after_an_off_thread_unset_returns)
{
    SocketPair holder{};
    SocketPair gate{};
    SocketPair target{};
    Probe probe{};
    ASSERT_TRUE(holder.Open());
    ASSERT_TRUE(gate.Open());
    ASSERT_TRUE(target.Open());
    ASSERT_TRUE(probe.open());
    const int holder_fd{holder.ReadFd()};
    const int gate_fd{gate.ReadFd()};
    const int target_fd{target.ReadFd()};
    LoopGate holder_gate{};
    LoopGate batch_gate{};
    std::atomic<std::uint32_t> target_starts{0U};

    Multiplexer mux{};
    mux.start();
    // The Linux backend dispatches a batch in registration-slot order, and a fresh multiplexer fills its slots in set()
    // order, so the target sits behind the held gate in the batch there.
    mux.set(gate_fd, [&](bool) {
        DrainByte(gate_fd);
        batch_gate.pass();
    });
    // The target is never drained, so it stays readable for the whole case.
    mux.set(target_fd, [&target_starts](bool) {
        ++target_starts;
    });
    mux.set(holder_fd, [&](bool) {
        DrainByte(holder_fd);
        holder_gate.pass();
    });
    mux.set(probe.fd(), probe.callback());

    // The loop is held outside the wait while both fds become readable, so the next wait reports them together.
    std::unique_lock<std::mutex> holding{holder_gate.close()};
    PokeByte(holder.WriteFd());
    ASSERT_TRUE(holder_gate.reached()) << "the holder never ran";
    std::unique_lock<std::mutex> gating{batch_gate.close()};
    PokeByte(target.WriteFd());
    PokeByte(gate.WriteFd());
    holding.unlock();
    ASSERT_TRUE(batch_gate.reached()) << "the gate never ran";
#if defined(LINUX)
    ASSERT_EQ(target_starts.load(), 0U) << "the target ran before the gate, so the batch order is not the slot order";
#endif  // LINUX

    bool unset_returned{false};
    std::uint32_t starts_at_unset{0U};
    {
        OffThreadCall call{[&mux, target_fd]() {
            mux.unset(target_fd);
        }};
        // The target's callback is not running, so the unset() returns while the loop is still held in the gate.
        unset_returned = call.returns_by(event_deadline());
        starts_at_unset = target_starts.load();
        gating.unlock();
    }
    ASSERT_TRUE(unset_returned) << "unset() waited for the callback of another fd";

    ASSERT_TRUE(probe.completes_pass());
    ASSERT_TRUE(probe.completes_pass());
    EXPECT_EQ(target_starts.load(), starts_at_unset) << "a callback started after unset() returned";
}

// [Multiplexer::unset] and [Multiplexer::set]: a reused fd number dispatches to the callback of the latest set().
TEST(Multiplexer, reused_fd_dispatches_only_the_new_callback)
{
    SocketPair old_pair{};
    SocketPair new_pair{};
    ASSERT_TRUE(old_pair.Open());
    ASSERT_TRUE(new_pair.Open());
    const int fd{old_pair.ReadFd()};
    std::atomic<std::uint32_t> old_calls{0U};
    std::atomic<std::uint32_t> new_calls{0U};
    std::atomic<std::uint32_t> timeouts{0U};
    std::atomic<std::uint32_t> timeouts_at_first_new_call{kNotRecorded};

    Multiplexer mux{};
    mux.start();
    mux.set(fd, counting(fd, old_calls));

    PokeByte(old_pair.WriteFd());
    ASSERT_TRUE(reaches(old_calls, 1U)) << "the old callback never ran";

    // Only an idle pass calls the timeout callback.
    // A count above zero shows a set() that the loop watched only after waiting out the idle second.
    mux.set_timeout([&timeouts]() {
        ++timeouts;
    });
    ASSERT_TRUE(reuse_fd(mux, old_pair, new_pair, [&, fd](bool) {
        DrainByte(fd);
        if (new_calls.load() == 0U)
        {
            timeouts_at_first_new_call = timeouts.load();
        }
        ++new_calls;
    }));

    // Several rounds, so a registration that comes and goes would show up as a round that never dispatches.
    for (std::uint32_t round{1U}; round <= 3U; ++round)
    {
        PokeByte(new_pair.WriteFd());
        ASSERT_TRUE(reaches(new_calls, round)) << "the new callback stopped running in round " << round;
    }
    EXPECT_EQ(old_calls.load(), 1U) << "the old callback ran for the reused fd";
    EXPECT_EQ(timeouts_at_first_new_call.load(), 0U) << "the loop watched the new set() only after an idle pass";
}

// [Multiplexer::unset] and [Multiplexer::set]: readiness of the old socket that the loop has not dispatched yet does
// not reach the callback set for the reused fd number.
// On QNX the old socket's pulse is already queued when the number is reused, so this checks that it is dropped.
TEST(Multiplexer, reused_fd_does_not_dispatch_the_old_sockets_pending_readiness)
{
    SocketPair gate_pair{};
    SocketPair old_pair{};
    SocketPair new_pair{};
    Probe probe{};
    ASSERT_TRUE(gate_pair.Open());
    ASSERT_TRUE(old_pair.Open());
    ASSERT_TRUE(new_pair.Open());
    ASSERT_TRUE(probe.open());
    const int gate_fd{gate_pair.ReadFd()};
    const int fd{old_pair.ReadFd()};
    LoopGate gate{};
    std::atomic<std::uint32_t> old_calls{0U};
    std::atomic<std::uint32_t> new_calls{0U};

    Multiplexer mux{};
    mux.start();
    mux.set(probe.fd(), probe.callback());
    mux.set(gate_fd, [&](bool) {
        DrainByte(gate_fd);
        gate.pass();
    });
    mux.set(fd, counting(fd, old_calls));

    // The old socket becomes readable while the loop is held, so its readiness is pending when the number is reused.
    std::unique_lock<std::mutex> hold{gate.close()};
    PokeByte(gate_pair.WriteFd());
    ASSERT_TRUE(gate.reached()) << "the gate never ran";
    PokeByte(old_pair.WriteFd());

    bool calls_returned{false};
    std::atomic_bool moved{false};
    {
        OffThreadCall call{[&]() {
            moved = reuse_fd(mux, old_pair, new_pair, counting(fd, new_calls));
        }};
        // The old callback is not running, so neither unset() nor set() has anything to wait for.
        calls_returned = call.returns_by(event_deadline());
        hold.unlock();
    }
    ASSERT_TRUE(calls_returned) << "unset() or set() waited for the callback of another fd";
    ASSERT_TRUE(moved.load());

    ASSERT_TRUE(probe.completes_pass());
    ASSERT_TRUE(probe.completes_pass());
    EXPECT_EQ(new_calls.load(), 0U) << "the new callback ran although the new socket had no data";
    EXPECT_EQ(old_calls.load(), 0U) << "the old callback ran after unset() returned";

    PokeByte(new_pair.WriteFd());
    EXPECT_TRUE(reaches(new_calls, 1U)) << "the set() on the reused fd number was never registered";
}

// [Multiplexer::unset]: only a call from another thread waits for a running callback, so the fd's own callback can
// call it.
TEST(Multiplexer, unset_from_the_fds_own_callback_returns_and_stops_its_dispatch)
{
    SocketPair pair{};
    Probe probe{};
    ASSERT_TRUE(pair.Open());
    ASSERT_TRUE(probe.open());
    const int fd{pair.ReadFd()};
    std::atomic<std::uint32_t> calls{0U};
    std::atomic<std::uint32_t> unsets_returned{0U};

    Multiplexer mux{};
    mux.start();
    mux.set(probe.fd(), probe.callback());
    // The fd is never drained, so only the unset() can keep the callback from running again.
    mux.set(fd, [&](bool) {
        ++calls;
        mux.unset(fd);
        ++unsets_returned;
    });

    PokeByte(pair.WriteFd());
    ASSERT_TRUE(reaches(unsets_returned, 1U)) << "unset() from the fd's own callback did not return";

    ASSERT_TRUE(probe.completes_pass());
    ASSERT_TRUE(probe.completes_pass());
    EXPECT_EQ(calls.load(), 1U) << "the callback ran again after it unset its own fd";
}

// [Multiplexer::stop]: wakes the thread and makes it leave its loop. join() waits for that.
TEST(Multiplexer, stop_and_join_return_well_within_the_idle_wait)
{
    Probe probe{};
    ASSERT_TRUE(probe.open());

    Multiplexer mux{};
    mux.start();
    mux.set(probe.fd(), probe.callback());
    // A completed pass puts the loop back into a fresh 1000 ms wait, which only a wake can cut short.
    ASSERT_TRUE(probe.completes_pass());

    const Clock::time_point stopping{Clock::now()};
    mux.stop();
    mux.join();
    const auto stopped_after_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - stopping).count();

    EXPECT_LT(stopped_after_ms, 500) << "stop() did not wake the loop";
}

// [Multiplexer::~Multiplexer]: stops and joins the thread, then destroys every callback that is still set.
TEST(Multiplexer, destroying_a_running_multiplexer_joins_and_dispatches_nothing_afterwards)
{
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());
    const int fd{pair.ReadFd()};
    LoopGate gate{};
    std::atomic<std::uint32_t> calls{0U};
    std::atomic_bool in_callback{false};
    // Copied into the callback, so its use count shows whether the callback is still stored.
    const auto token = std::make_shared<int>(0);

    auto mux = std::make_unique<Multiplexer>();
    mux->start();
    mux->set(fd, [&, token](bool) {
        DrainByte(fd);
        ++calls;
        in_callback = true;
        gate.pass();
        in_callback = false;
    });

    std::unique_lock<std::mutex> hold{gate.close()};
    PokeByte(pair.WriteFd());
    ASSERT_TRUE(gate.reached()) << "the callback never ran";

    bool returned_while_callback_ran{false};
    bool destroyed{false};
    std::atomic_bool callback_ran_past_destruction{false};
    {
        OffThreadCall call{[&]() {
            mux.reset();
            callback_ran_past_destruction = in_callback.load();
        }};
        // A destructor that joins cannot return while the callback is held.
        returned_while_callback_ran = call.returns_by(Clock::now() + kQuietWindow);
        hold.unlock();
        destroyed = call.returns_by(event_deadline());
    }
    ASSERT_TRUE(destroyed) << "the destructor did not return after the callback completed";
    EXPECT_FALSE(returned_while_callback_ran) << "the destructor returned while a callback ran";
    EXPECT_FALSE(callback_ran_past_destruction.load()) << "the destructor returned while a callback ran";

    const std::uint32_t calls_at_destruction{calls.load()};
    PokeByte(pair.WriteFd());
    EXPECT_FALSE(WaitFor(
        [&]() {
            return calls.load() != calls_at_destruction;
        },
        Clock::now() + kQuietWindow))
        << "a callback ran after the destructor returned";
    EXPECT_EQ(token.use_count(), 1) << "the destructor left the callback stored";
}

// [Multiplexer::unset]: called from another thread, it destroys the callback, also after join() ended the loop.
TEST(Multiplexer, unset_after_join_returns_and_destroys_the_callback)
{
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());
    const int fd{pair.ReadFd()};
    const auto token = std::make_shared<int>(0);

    Multiplexer mux{};
    mux.start();
    mux.set(fd, [fd, token](bool) {
        DrainByte(fd);
    });
    mux.stop();
    mux.join();

    bool unset_returned{false};
    {
        OffThreadCall call{[&mux, fd]() {
            mux.unset(fd);
        }};
        unset_returned = call.returns_by(event_deadline());
    }
    ASSERT_TRUE(unset_returned) << "unset() after join() waited for a loop that no longer runs";
    EXPECT_EQ(token.use_count(), 1) << "unset() left the callback stored";
}

// [Multiplexer::set]: `callback` receives `false` for hang-up, and detects end of stream from a read that returns 0.
TEST(Multiplexer, delivers_a_peer_hang_up_as_callback_false)
{
    SocketPair pair{};
    ASSERT_TRUE(pair.Open());
    const int fd{pair.ReadFd()};
    std::atomic<std::uint32_t> calls{0U};
    std::atomic_bool got_true{false};
    std::atomic<ssize_t> read_result{-2};

    Multiplexer mux{};
    mux.start();
    // Unset at once, because a hung-up fd stays readable and would otherwise be dispatched on every pass.
    mux.set(fd, [&](const bool arg) {
        std::uint8_t byte{0U};
        read_result = ::read(fd, &byte, sizeof(byte));
        if (arg)
        {
            got_true = true;
        }
        mux.unset(fd);
        ++calls;
    });

    pair.WriteEnd().Close();
    ASSERT_TRUE(reaches(calls, 1U)) << "the hang-up was not dispatched";

    EXPECT_FALSE(got_true.load()) << "the hang-up was delivered as callback(true)";
    EXPECT_EQ(read_result.load(), 0) << "the read in the callback did not find end of stream";
}

// [Multiplexer::set]: a negative fd and an empty callback are logged and ignored.
TEST(Multiplexer, set_of_a_negative_fd_or_an_empty_callback_registers_nothing)
{
    Probe probe{};
    SocketPair pair{};
    ASSERT_TRUE(probe.open());
    ASSERT_TRUE(pair.Open());
    const int fd{pair.ReadFd()};
    std::atomic<std::uint32_t> negative_calls{0U};
    std::atomic<std::uint32_t> later_calls{0U};
    const auto token = std::make_shared<int>(0);

    Multiplexer mux{};
    mux.start();
    mux.set(probe.fd(), probe.callback());
    mux.set(-1, [&negative_calls, token](bool) {
        ++negative_calls;
    });
    mux.set(fd, std::function<void(bool)>{});

    EXPECT_EQ(token.use_count(), 1) << "set(-1) stored its callback";
    // A stored empty callback throws std::bad_function_call on the loop thread, which terminates the test binary.
    PokeByte(pair.WriteFd());
    ASSERT_TRUE(probe.completes_pass()) << "the loop stopped dispatching after an ignored set()";
    ASSERT_TRUE(probe.completes_pass());
    EXPECT_EQ(negative_calls.load(), 0U);

    // An ignored set() leaves nothing behind that refuses a later set() of the same fd.
    mux.set(fd, counting(fd, later_calls));
    EXPECT_TRUE(reaches(later_calls, 1U)) << "set() after an ignored set(fd, empty callback) did not register";
}

// [Multiplexer::unset]: an fd that is not set is ignored.
TEST(Multiplexer, unset_of_an_unknown_or_invalid_fd_is_a_no_op)
{
    Probe probe{};
    SocketPair unknown{};
    ASSERT_TRUE(probe.open());
    ASSERT_TRUE(unknown.Open());
    const int unknown_fd{unknown.ReadFd()};
    std::atomic<std::uint32_t> unknown_calls{0U};

    Multiplexer mux{};
    mux.start();
    mux.set(probe.fd(), probe.callback());

    mux.unset(unknown_fd);
    mux.unset(-1);

    ASSERT_TRUE(probe.completes_pass()) << "unset() of an fd never set withdrew another registration";
    // A no-op leaves nothing behind that refuses a later set() of the same fd.
    mux.set(unknown_fd, counting(unknown_fd, unknown_calls));
    PokeByte(unknown.WriteFd());
    EXPECT_TRUE(reaches(unknown_calls, 1U)) << "set() after unset() of an fd never set did not register";
}

// [Multiplexer::set]: an fd that is already set is logged and ignored, so the first callback stays.
TEST(Multiplexer, second_set_of_a_set_fd_keeps_the_first_callback)
{
    SocketPair pair{};
    Probe probe{};
    ASSERT_TRUE(pair.Open());
    ASSERT_TRUE(probe.open());
    const int fd{pair.ReadFd()};
    std::atomic<std::uint32_t> first_calls{0U};
    std::atomic<std::uint32_t> second_calls{0U};
    const auto token = std::make_shared<int>(0);

    Multiplexer mux{};
    mux.start();
    mux.set(probe.fd(), probe.callback());
    mux.set(fd, counting(fd, first_calls));
    mux.set(fd, [&second_calls, fd, token](bool) {
        DrainByte(fd);
        ++second_calls;
    });

    EXPECT_EQ(token.use_count(), 1) << "the second set() stored its callback";
    PokeByte(pair.WriteFd());
    ASSERT_TRUE(reaches(first_calls, 1U)) << "the first callback is no longer dispatched";
    EXPECT_EQ(second_calls.load(), 0U) << "the second callback was dispatched";

    // One unset() withdraws the fd, so the ignored set() left no second registration behind.
    mux.unset(fd);
    const std::uint32_t first_calls_at_unset{first_calls.load()};
    PokeByte(pair.WriteFd());
    ASSERT_TRUE(probe.completes_pass());
    ASSERT_TRUE(probe.completes_pass());
    EXPECT_EQ(first_calls.load(), first_calls_at_unset) << "the fd was dispatched after unset()";
    EXPECT_EQ(second_calls.load(), 0U) << "the second callback was dispatched after unset()";
}

// [Multiplexer::set]: a full reactor is logged and ignored.
TEST(Multiplexer, set_on_a_full_reactor_logs_and_registers_nothing)
{
    // The case holds about 270 descriptors: the probe, kReactorCapacity registrations and one more pair.
    FdLimit fd_limit{};
    if (!fd_limit.ensure_at_least(1024U))
    {
        GTEST_SKIP() << "RLIMIT_NOFILE cannot be raised to 1024 descriptors";
    }

    Probe probe{};
    SocketPair filler{};
    SocketPair last{};
    SocketPair overflow{};
    ASSERT_TRUE(probe.open());
    ASSERT_TRUE(filler.Open());
    ASSERT_TRUE(last.Open());
    ASSERT_TRUE(overflow.Open());

    // Duplicates of one never-poked socket fill all but two slots, so they are never dispatched.
    std::vector<UniqueFd> duplicates{};
    for (std::size_t i{0U}; i < kReactorCapacity - 2U; ++i)
    {
        UniqueFd duplicate{::dup(filler.ReadFd())};
        ASSERT_TRUE(duplicate.IsValid()) << "dup() failed after " << i << " descriptors";
        duplicates.push_back(std::move(duplicate));
    }
    const int last_fd{last.ReadFd()};
    const int overflow_fd{overflow.ReadFd()};
    std::atomic<std::uint32_t> last_calls{0U};
    std::atomic<std::uint32_t> overflow_calls{0U};
    const auto token = std::make_shared<int>(0);

    Multiplexer mux{};
    mux.start();
    mux.set(probe.fd(), probe.callback());
    for (const UniqueFd& duplicate : duplicates)
    {
        mux.set(duplicate.Get(), [](bool) {});
    }
    // The registration that fills the reactor.
    mux.set(last_fd, counting(last_fd, last_calls));

    std::string log{};
    {
        CoutCapture capture{};
        mux.set(overflow_fd, [&overflow_calls, overflow_fd, token](bool) {
            DrainByte(overflow_fd);
            ++overflow_calls;
        });
        log = capture.str();
    }

    EXPECT_EQ(token.use_count(), 1) << "set() on a full reactor stored its callback";
    EXPECT_NE(log.find("[Error]"), std::string::npos) << "set() on a full reactor logged no error: " << log;

    PokeByte(last.WriteFd());
    EXPECT_TRUE(reaches(last_calls, 1U)) << "the registration that filled the reactor is not dispatched";
    PokeByte(overflow.WriteFd());
    ASSERT_TRUE(probe.completes_pass());
    ASSERT_TRUE(probe.completes_pass());
    EXPECT_EQ(overflow_calls.load(), 0U) << "the fd set on a full reactor was dispatched";
}

#if defined(LINUX)
// [Multiplexer::Multiplexer]: throws std::runtime_error when the reactor cannot get its OS resources.
// The Linux reactor opens a wake-up pipe, which fails once no descriptor number is left below RLIMIT_NOFILE.
TEST(Multiplexer, constructor_throws_runtime_error_when_the_reactor_gets_no_descriptors)
{
    // The lowest free descriptor number, so a limit set to it leaves no descriptor to open.
    const int lowest_free{::open("/dev/null", O_RDONLY)};
    ASSERT_GE(lowest_free, 0);
    static_cast<void>(::close(lowest_free));

    bool threw_runtime_error{false};
    bool threw_something_else{false};
    {
        FdLimit fd_limit{};
        ASSERT_TRUE(fd_limit.set_soft(static_cast<rlim_t>(lowest_free)));
        try
        {
            Multiplexer mux{};
        }
        catch (const std::runtime_error&)
        {
            threw_runtime_error = true;
        }
        catch (...)
        {
            threw_something_else = true;
        }
    }

    EXPECT_TRUE(threw_runtime_error);
    EXPECT_FALSE(threw_something_else) << "the constructor threw an exception other than std::runtime_error";
}
#endif  // LINUX
