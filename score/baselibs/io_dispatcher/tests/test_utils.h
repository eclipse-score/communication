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
 ********************************************************************************/
#ifndef IO_DISPATCHER_TESTS_TEST_UTILS_H
#define IO_DISPATCHER_TESTS_TEST_UTILS_H

/**
 * @file test_utils.h
 * @brief Descriptor ownership, socket construction and reactor pumping shared by the io_dispatcher tests.
 *
 * Reactors are built through MakeIoDispatcher(), so the same tests build against either backend.
 * @warning Every descriptor a test opens belongs to a UniqueFd or a SocketPair.
 * A failed ASSERT returns from the test body at once, so a descriptor closed by a trailing statement leaks.
 */

#include "score/io_dispatcher/io_dispatcher.h"
#include "score/io_dispatcher/io_dispatcher_factory.h"
#include "score/io_dispatcher/io_dispatcher_types.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace test_utils
{

using Clock = std::chrono::steady_clock;

/**
 * @brief Owns one file descriptor and closes it on destruction.
 */
class UniqueFd final
{
  public:
    UniqueFd() noexcept = default;

    explicit UniqueFd(const int fd) noexcept : fd_{fd} {}

    ~UniqueFd() noexcept
    {
        Close();
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_{std::exchange(other.fd_, io_dispatcher::kInvalidNativeHandle)} {}

    UniqueFd& operator=(UniqueFd&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            fd_ = std::exchange(other.fd_, io_dispatcher::kInvalidNativeHandle);
        }
        return *this;
    }

    int Get() const noexcept
    {
        return fd_;
    }

    bool IsValid() const noexcept
    {
        return fd_ >= 0;
    }

    /**
     * @brief Closes the descriptor early, for a test that must observe the effect of the close.
     */
    void Close() noexcept
    {
        if (IsValid())
        {
            score::cpp::ignore = ::close(fd_);
            fd_ = io_dispatcher::kInvalidNativeHandle;
        }
    }

  private:
    int fd_{io_dispatcher::kInvalidNativeHandle};
};

/**
 * @brief A connected stream-socket pair standing in for an established TCP connection.
 *
 * ReadFd() is the end registered in the reactor; WriteFd() is the peer a test writes to.
 * Both ends are non-blocking, so a read the reactor should never have prompted fails a test instead of hanging it.
 */
class SocketPair final
{
  public:
    /**
     * @brief Creates the pair.
     * @return false when socketpair() fails; the pair stays unusable and IsOpen() stays false.
     */
    bool Open() noexcept
    {
        int fds[2] = {io_dispatcher::kInvalidNativeHandle, io_dispatcher::kInvalidNativeHandle};
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
        {
            return false;
        }
        read_fd_ = UniqueFd{fds[0]};
        write_fd_ = UniqueFd{fds[1]};
        for (const int fd : fds)
        {
            const int flags{::fcntl(fd, F_GETFL, 0)};
            score::cpp::ignore = ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
        return true;
    }

    bool IsOpen() const noexcept
    {
        return read_fd_.IsValid() && write_fd_.IsValid();
    }

    int ReadFd() const noexcept
    {
        return read_fd_.Get();
    }

    int WriteFd() const noexcept
    {
        return write_fd_.Get();
    }

    UniqueFd& ReadEnd() noexcept
    {
        return read_fd_;
    }

    UniqueFd& WriteEnd() noexcept
    {
        return write_fd_;
    }

  private:
    UniqueFd read_fd_{};
    UniqueFd write_fd_{};
};

inline io_dispatcher::EventTypes EmptyEvent() noexcept
{
    return io_dispatcher::EventTypes{};
}

inline io_dispatcher::EventTypes ReadEvent() noexcept
{
    return io_dispatcher::EventTypes{}.SetReadEvent(true);
}

inline io_dispatcher::EventTypes WriteEvent() noexcept
{
    return io_dispatcher::EventTypes{}.SetWriteEvent(true);
}

inline io_dispatcher::EventTypes ReadWriteEvent() noexcept
{
    return io_dispatcher::EventTypes{}.SetReadEvent(true).SetWriteEvent(true);
}

/**
 * @brief Builds a reactor for max_num_event_sources sources through the platform-agnostic factory.
 * @param[in] tag Stamped into every handle the reactor issues.
 * @return nullptr when the factory fails: max_num_event_sources above the backend limit, a tag above
 *         kMaxDispatcherTag, or no wake-up channel.
 */
inline std::unique_ptr<io_dispatcher::IoDispatcher> MakeReactor(const std::uint16_t max_num_event_sources,
                                                                const io_dispatcher::DispatcherTag tag = 0U)
{
    auto reactor = io_dispatcher::MakeIoDispatcher(max_num_event_sources, tag);
    if (!reactor.has_value())
    {
        return nullptr;
    }
    return std::move(reactor).value();
}

/**
 * @brief Writes one byte, making the peer's descriptor readable.
 */
inline void PokeByte(const int fd) noexcept
{
    const std::uint8_t byte{0x5AU};
    score::cpp::ignore = ::write(fd, &byte, sizeof(byte));
}

/**
 * @brief Reads one byte, so a descriptor a callback did not drain stops being readable.
 */
inline void DrainByte(const int fd) noexcept
{
    std::uint8_t byte{0};
    score::cpp::ignore = ::read(fd, &byte, sizeof(byte));
}

/**
 * @brief Consumes the wake-up that Register and the monitored-event calls queue.
 * @pre Call before asserting kTimeout, otherwise the wake-up is reported instead of the idle state.
 */
inline void DrainWakeup(io_dispatcher::IoDispatcher& reactor) noexcept
{
    score::cpp::ignore = reactor.HandleEvents(std::chrono::milliseconds{0});
}

/**
 * @brief Drives the reactor until done is set or max_iterations waits have elapsed.
 * @return The final state of done; a wait that reports an error also returns false.
 * @note A wake-up can end the first wait before a ready descriptor is reported, so one iteration is not enough.
 */
inline bool PumpUntil(io_dispatcher::IoDispatcher& reactor,
                      const std::atomic_bool& done,
                      const int max_iterations) noexcept
{
    for (int i = 0; (i < max_iterations) && (!done.load()); ++i)
    {
        if (!reactor.HandleEvents(std::chrono::milliseconds{300}).has_value())
        {
            return false;
        }
    }
    return done.load();
}

/**
 * @brief Drives the reactor through exactly `passes` waits of `timeout` each.
 * @return false as soon as a wait reports an error.
 * @note Used before asserting that no further callback ran, so a late event still has a pass to arrive in.
 */
inline bool PumpPasses(io_dispatcher::IoDispatcher& reactor,
                       const int passes,
                       const std::chrono::milliseconds timeout) noexcept
{
    for (int i = 0; i < passes; ++i)
    {
        if (!reactor.HandleEvents(timeout).has_value())
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Polls predicate until it holds or deadline passes.
 * @return The final state of the predicate.
 * @note Sleeps between checks; it never calls HandleEvents(), so it cannot advance a reactor itself.
 */
template <typename Predicate>
inline bool WaitFor(Predicate predicate, const Clock::time_point deadline)
{
    while (!predicate())
    {
        if (Clock::now() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return true;
}

/**
 * @brief Drives a reactor on a thread of its own for as long as the object lives.
 *
 * @pre The reactor outlives the ReactorThread, and no other thread calls HandleEvents() meanwhile.
 */
class ReactorThread final
{
  public:
    explicit ReactorThread(io_dispatcher::IoDispatcher& reactor,
                           const std::chrono::milliseconds timeout = std::chrono::milliseconds{50})
        : reactor_{reactor}, thread_{[this, timeout]() {
              while (run_.load())
              {
                  score::cpp::ignore = reactor_.HandleEvents(timeout);
              }
          }}
    {
    }

    ~ReactorThread() noexcept
    {
        Stop();
    }

    ReactorThread(const ReactorThread&) = delete;
    ReactorThread& operator=(const ReactorThread&) = delete;
    ReactorThread(ReactorThread&&) = delete;
    ReactorThread& operator=(ReactorThread&&) = delete;

    /**
     * @brief Ends the pump and joins.
     * @note Idempotent, so the destructor can call it after a test already has.
     */
    void Stop() noexcept
    {
        if (thread_.joinable())
        {
            run_.store(false);
            reactor_.Unblock();
            thread_.join();
        }
    }

  private:
    io_dispatcher::IoDispatcher& reactor_;
    std::atomic_bool run_{true};
    std::thread thread_;
};

/**
 * @brief A UDP socket bound to an ephemeral loopback port, with its address written to bound_addr.
 * @return An invalid UniqueFd when socket(), bind() or getsockname() fails; bound_addr stays unwritten.
 */
inline UniqueFd MakeUdpSocket(sockaddr_in& bound_addr) noexcept
{
    UniqueFd fd{::socket(AF_INET, SOCK_DGRAM, 0)};
    if (!fd.IsValid())
    {
        return UniqueFd{};
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(fd.Get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        return UniqueFd{};
    }
    socklen_t len{sizeof(bound_addr)};
    if (::getsockname(fd.Get(), reinterpret_cast<sockaddr*>(&bound_addr), &len) != 0)
    {
        return UniqueFd{};
    }
    return fd;
}

/**
 * @brief A listening TCP socket on an ephemeral loopback port, with its address written to bound_addr.
 * @return An invalid UniqueFd when socket(), bind(), listen() or getsockname() fails; bound_addr stays unwritten.
 */
inline UniqueFd MakeTcpListener(sockaddr_in& bound_addr) noexcept
{
    UniqueFd fd{::socket(AF_INET, SOCK_STREAM, 0)};
    if (!fd.IsValid())
    {
        return UniqueFd{};
    }
    int one{1};
    score::cpp::ignore = ::setsockopt(fd.Get(), SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if ((::bind(fd.Get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) || (::listen(fd.Get(), 4) != 0))
    {
        return UniqueFd{};
    }
    socklen_t len{sizeof(bound_addr)};
    if (::getsockname(fd.Get(), reinterpret_cast<sockaddr*>(&bound_addr), &len) != 0)
    {
        return UniqueFd{};
    }
    return fd;
}

/**
 * @brief Opens up to requested socket pairs.
 * @return Fewer pairs than requested when the process runs out of descriptors; the caller must check the size.
 */
inline std::vector<SocketPair> MakePool(const std::size_t requested)
{
    std::vector<SocketPair> pool{};
    pool.reserve(requested);
    for (std::size_t i = 0U; i < requested; ++i)
    {
        SocketPair pair{};
        if (!pair.Open())
        {
            break;
        }
        pool.push_back(std::move(pair));
    }
    return pool;
}

}  // namespace test_utils

#endif  // IO_DISPATCHER_TESTS_TEST_UTILS_H
