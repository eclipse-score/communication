/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/message_passing/log/logging_callback.h"

#include "score/message_passing/log/log.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace score::message_passing
{
namespace
{

using namespace ::testing;

// A std::streambuf that records every write into a fixed, preallocated buffer without ever allocating memory.
// It also counts how many times the underlying stream buffer receives a write (xsputn/overflow). This lets a test
// prove that a whole log line is emitted in a single atomic write - the property that replaced the removed
// std::mutex and prevents concurrent log lines from interleaving character-by-character.
class RecordingStreamBuf final : public std::streambuf
{
  public:
    std::size_t WriteCalls() const noexcept
    {
        return write_calls_;
    }
    std::string_view View() const noexcept
    {
        return std::string_view{buffer_.data(), length_};
    }
    void Reset() noexcept
    {
        write_calls_ = 0U;
        length_ = 0U;
    }

  protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        ++write_calls_;
        const std::size_t count = std::min(static_cast<std::size_t>(n), buffer_.size() - length_);
        for (std::size_t i = 0U; i < count; ++i)
        {
            buffer_[length_ + i] = s[i];
        }
        length_ += count;
        return n;
    }

    int overflow(int ch) override
    {
        ++write_calls_;
        if ((ch != std::char_traits<char>::eof()) && (length_ < buffer_.size()))
        {
            buffer_[length_] = static_cast<char>(ch);
            ++length_;
        }
        return ch;
    }

  private:
    std::array<char, 8192U> buffer_{};
    std::size_t length_{0U};
    std::size_t write_calls_{0U};
};

// RAII helper redirecting std::cerr to a custom stream buffer for the duration of a test scope.
class CerrRedirect final
{
  public:
    explicit CerrRedirect(std::streambuf* const sink) noexcept : previous_{std::cerr.rdbuf(sink)} {}
    ~CerrRedirect() noexcept
    {
        static_cast<void>(std::cerr.rdbuf(previous_));
    }
    CerrRedirect(const CerrRedirect&) = delete;
    CerrRedirect& operator=(const CerrRedirect&) = delete;
    CerrRedirect(CerrRedirect&&) = delete;
    CerrRedirect& operator=(CerrRedirect&&) = delete;

  private:
    std::streambuf* previous_;
};

TEST(GetCerrLoggerTest, FormatsAllItemTypesAndAppendsNewline)
{
    RecordingStreamBuf sink{};
    const LoggingCallback logger = GetCerrLogger();

    std::int32_t anchor{0};
    const void* const pointer = &anchor;

    // Independent oracle for the implementation-defined pointer representation.
    std::ostringstream pointer_oracle{};
    pointer_oracle << pointer;
    const std::string expected = std::string{"TryOpenClientConnection "} + "-5" + "7" + pointer_oracle.str() + "\n";

    {
        const CerrRedirect redirect{&sink};
        LogInfo(logger, "TryOpenClientConnection ", std::int64_t{-5}, std::uint64_t{7}, pointer);
    }

    EXPECT_EQ(sink.View(), expected);
}

// Deterministic, host-CI regression guard for the root cause of the shutdown crash.
//
// The pre-fix logger serialised its output with a function-local `static std::mutex` precisely because it streamed
// each item separately (`std::cerr << item ...`), so concurrent log calls had to be locked to avoid interleaving.
// That shared static was then destroyed during shutdown (__cxa_finalize) while a background dispatch thread was
// still logging -> lock of a destroyed mutex -> EINVAL -> throw -> abort() in the no-exception build.
//
// The fix formats the whole line first and emits it with a single write, which is what makes the cross-thread mutex
// unnecessary. This test locks in that invariant: exactly ONE write per log call. It FAILS against the old
// per-item-streaming design (which produced multiple writes and therefore required the fatal static mutex) and
// PASSES against the fix - verified by temporarily reinstating the old implementation.
TEST(GetCerrLoggerTest, EmitsEachLineInASingleAtomicWrite)
{
    RecordingStreamBuf sink{};
    const LoggingCallback logger = GetCerrLogger();

    {
        const CerrRedirect redirect{&sink};
        LogInfo(logger, "TryOpenClientConnection ", std::uint64_t{42}, " suffix");
    }

    EXPECT_EQ(sink.WriteCalls(), 1U);
    EXPECT_EQ(sink.View(), std::string_view{"TryOpenClientConnection 42 suffix\n"});
}

// Over-long content must be truncated to the fixed buffer, never overflow it.
TEST(GetCerrLoggerTest, TruncatesOverlongLinesWithoutOverflow)
{
    RecordingStreamBuf sink{};
    const LoggingCallback logger = GetCerrLogger();

    const std::string huge(5000U, 'x');
    {
        const CerrRedirect redirect{&sink};
        LogInfo(logger, std::string_view{huge});
    }

    EXPECT_LE(sink.View().size(), 1024U);
    EXPECT_THAT(std::string{sink.View()}, Each('x'));
}

// End-to-end regression test for the concurrent scenario that triggered the crash: many threads logging at once,
// like the client and server dispatch threads. Output is captured through the real stderr file descriptor so that
// the production std::cerr -> stdio write path (and its atomicity) is exercised. Every emitted line must be one of
// the complete, expected tokens (no interleaving, no corruption) and the run must not throw or abort.
TEST(GetCerrLoggerTest, ConcurrentLoggingProducesCompleteNonInterleavedLines)
{
    constexpr std::size_t kThreads{8U};
    constexpr std::size_t kIterations{2000U};

    std::set<std::string> expected_tokens{};
    for (std::size_t k = 0U; k < kThreads; ++k)
    {
        expected_tokens.insert(std::string{"TAG"} + std::to_string(k) + "-END");
    }

    std::FILE* const capture = std::tmpfile();
    ASSERT_NE(capture, nullptr);
    const int capture_fd = ::fileno(capture);

    std::cerr.flush();
    std::fflush(stderr);
    const int saved_stderr = ::dup(STDERR_FILENO);
    ASSERT_GE(saved_stderr, 0);
    ASSERT_GE(::dup2(capture_fd, STDERR_FILENO), 0);

    {
        const LoggingCallback logger = GetCerrLogger();
        std::vector<std::thread> workers{};
        workers.reserve(kThreads);
        for (std::size_t k = 0U; k < kThreads; ++k)
        {
            workers.emplace_back([&logger, k]() noexcept {
                for (std::size_t i = 0U; i < kIterations; ++i)
                {
                    LogInfo(logger, "TAG", std::uint64_t{k}, "-END");
                }
            });
        }
        for (auto& worker : workers)
        {
            worker.join();
        }
    }

    std::cerr.flush();
    std::fflush(stderr);
    ASSERT_GE(::dup2(saved_stderr, STDERR_FILENO), 0);
    static_cast<void>(::close(saved_stderr));

    std::string content{};
    static_cast<void>(std::fseek(capture, 0L, SEEK_SET));
    std::array<char, 4096U> chunk{};
    for (;;)
    {
        const std::size_t read = std::fread(chunk.data(), 1U, chunk.size(), capture);
        if (read == 0U)
        {
            break;
        }
        content.append(chunk.data(), read);
    }
    static_cast<void>(std::fclose(capture));

    // The captured stream is the raw stderr file descriptor, so under a sanitizer build it also contains the
    // sanitizer runtime's own diagnostics (e.g. "==<pid>==T3: FakeStack created ...", "==<pid>==T3 exited") emitted
    // with verbosity=1. Those lines are not produced by the logger and must be ignored; every sanitizer message is
    // prefixed with "==<pid>==", a prefix no logger line or interleaved TAG fragment can start with. Any real
    // interleaving would still surface as a non-"==" line that is not an expected token and thus fail the check.
    std::size_t line_count{0U};
    std::istringstream lines{content};
    std::string line{};
    while (std::getline(lines, line))
    {
        if (std::string_view{line}.substr(0U, 2U) == "==")
        {
            continue;
        }
        ++line_count;
        EXPECT_EQ(expected_tokens.count(line), 1U) << "unexpected or interleaved line: '" << line << "'";
    }

    EXPECT_EQ(line_count, kThreads * kIterations);
}

// -------------------------------------------------------------------------------------------------------------------
// Regression test for the actual crash: the static-mutex shutdown race.
//
// The pre-fix logger serialised its output with a function-local `static std::mutex`. That static is registered for
// destruction with __cxa_atexit and is therefore torn down during process shutdown (std::exit -> __cxa_finalize).
// A background dispatch thread (e.g. a ClientConnection retrying TryOpenClientConnection) could still be logging at
// that moment; locking the already-destroyed mutex returned EINVAL, libc++ threw std::system_error, and in the
// no-exception build __cxa_allocate_exception called abort() -> SIGABRT. This is exactly the xnm coredump.
//
// This test reproduces that scenario in a forked child: a detached thread keeps logging through the fallback logger
// while main() calls std::exit(0), running static destruction concurrently. With the fix the logger holds no static
// state that gets destroyed, so the child must terminate normally (exit code 0) instead of aborting.
//
// Why the destruction order is guaranteed (and why the worker is NOT torn down first): std::exit does not unwind
// the stack, so the automatic `std::thread worker` handle's destructor never runs ([support.start.term]: "no
// destructors are called for objects with automatic storage duration"). std::exit also does not join or stop other
// threads, nor run the thread-local destructors of threads other than the calling one - so the detached worker keeps
// executing concurrently through static destruction. The logger's mutex is a function-local `static` (static storage
// duration, shared across threads), not a `thread_local`, so it is destroyed in the static-destruction phase while
// the worker is still logging. The "last thread-local destructor is sequenced-before the first static destructor"
// rule applies only to the exiting thread's `thread_local` objects and is therefore irrelevant here. Accessing the
// static after its destruction is exactly the UB the standard warns about (the reason threads must be joined before
// exit) - i.e. precisely the bug under test.
//
// Note on portability: a literal SIGABRT reproduction of the *old* code is platform-dependent - glibc's
// pthread_mutex_lock on a destroyed plain mutex succeeds instead of returning EINVAL, so on the host this test
// passes even against the buggy implementation (verified empirically); only the QNX no-exception runtime aborts.
// The deterministic, host-observable regression guard for the root cause is therefore
// EmitsEachLineInASingleAtomicWrite above (the single-write invariant that removed the fatal static mutex); this
// test additionally exercises the exact runtime scenario and asserts the invariant the fix guarantees.
TEST(GetCerrLoggerShutdownDeathTest, LoggingThreadSurvivesStaticDestructionAtExit)
{
    EXPECT_EXIT(
        {
            // Silence the fallback logger's output inside the forked child.
            static_cast<void>(std::freopen("/dev/null", "w", stderr));

            const LoggingCallback logger = GetCerrLogger();
            std::atomic<bool> started{false};
            std::thread worker([&logger, &started]() noexcept {
                started.store(true, std::memory_order_release);
                for (;;)
                {
                    LogInfo(logger, "TryOpenClientConnection ", std::uint64_t{1});
                }
            });
            worker.detach();
            // Detached: std::exit neither joins nor destroys this thread, and it never runs the automatic `worker`
            // handle's destructor, so the thread keeps logging while the logger's function-local `static` is torn
            // down - the destruction order that surfaces the original crash.
            while (!started.load(std::memory_order_acquire))
            {
            }
            // Give the worker time to be deep inside the logger while statics are torn down.
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::exit(0);
        },
        ::testing::ExitedWithCode(0),
        "");
}

}  // namespace
}  // namespace score::message_passing
