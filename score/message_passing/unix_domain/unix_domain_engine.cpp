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
#include "score/message_passing/unix_domain/unix_domain_engine.h"

#include "score/message_passing/unix_domain/unix_domain_socket_address.h"

#include <future>

namespace score
{
namespace message_passing
{

UnixDomainEngine::UnixDomainEngine(score::cpp::pmr::memory_resource* memory_resource, LoggingCallback logger) noexcept
    : memory_resource_{memory_resource},
      os_resources_{GetDefaultOsResources(memory_resource)},
      logger_{std::move(logger)},
      quit_flag_{false},
      poll_fds_{memory_resource},
      poll_endpoints_{memory_resource},
      posix_receive_buffer_{memory_resource}
{
    os_resources_.unistd->pipe(pipe_fds_.data());

    // Normally, during the application lifecycle initialization, LifeCycleManager blocks the SIGTERM on the main
    // thread and creates a separate thread that catches all the SIGTERM signals coming to the process. The other
    // threads created after that will inherit the sigmask of the main thread with SIGTERM blocked.
    // However, LifeCycleManager starts using Logging before it blocks SIGTERM on the main thread. When this happens,
    // Logging will initialize Message Passing, which will create the Message Passing background thread with a sigmask
    // inherited without SIGTERM being blocked yet.
    // Thus, we need to mask SIGTERM for this thread specifically, to let the LifeCycleManager desicated SIGTERM
    // thread do its job.
    sigset_t new_set;
    sigset_t old_set;
    // the signal functions below, used with the parameters below, can only return EOK
    score::cpp::ignore = os_resources_.signal->SigEmptySet(new_set);
    score::cpp::ignore = os_resources_.signal->AddTerminationSignal(new_set);
    score::cpp::ignore = os_resources_.signal->PthreadSigMask(SIG_BLOCK, new_set, old_set);
    {
        std::lock_guard acquire{thread_mutex_};  // postpone RunOnThread() till we assign thread_
        thread_ = std::thread([this]() noexcept {
            {
                std::lock_guard release{thread_mutex_};  // guarantees that this->thread_ is already assigned
            }
            RunOnThread();
        });
    }
    score::cpp::ignore = os_resources_.signal->PthreadSigMask(SIG_SETMASK, old_set);
}

UnixDomainEngine::~UnixDomainEngine() noexcept
{
    SendPipeEvent(PipeEvent::QUIT);
    thread_.join();
    os_resources_.unistd->close(pipe_fds_[0]);
    os_resources_.unistd->close(pipe_fds_[1]);
}

score::cpp::expected<std::int32_t, score::os::Error> UnixDomainEngine::TryOpenClientConnection(
    std::string_view identifier) noexcept
{
    const auto fd_expected = os_resources_.socket->socket(score::os::Socket::Domain::kUnix, SOCK_STREAM, 0);
    if (!fd_expected.has_value())
    {
        return score::cpp::make_unexpected(fd_expected.error());
    }

    const std::int32_t client_fd = fd_expected.value();
    detail::UnixDomainSocketAddress addr{identifier, true};

    const auto connect_expected = os_resources_.socket->connect(client_fd, addr.data(), addr.size());
    if (!connect_expected.has_value())
    {
        os_resources_.unistd->close(client_fd);
        return score::cpp::make_unexpected(connect_expected.error());
    }
    return client_fd;
}

void UnixDomainEngine::CloseClientConnection(std::int32_t client_fd) noexcept
{
    os_resources_.unistd->close(client_fd);
}

void UnixDomainEngine::RegisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(IsOnCallbackThread());

    if (posix_receive_buffer_.size() < endpoint.max_receive_size)
    {
        posix_receive_buffer_.resize(endpoint.max_receive_size);
    }

    std::int16_t events = 0;
    if (!endpoint.input.empty())
    {
        events |= POLLIN;
    }
    if (!endpoint.output.empty())
    {
        // TODO: not used/not supported yet
        events |= POLLOUT;
    }
    const auto found = std::find_if(poll_fds_.begin(), poll_fds_.end(), [](pollfd& poll) noexcept {
        return poll.fd < 0;
    });
    if (found != poll_fds_.end())
    {
        *found = {endpoint.fd, events, 0};
        std::size_t i = static_cast<std::size_t>(std::distance(poll_fds_.begin(), found));
        poll_endpoints_[i] = &endpoint;
    }
    else
    {
        poll_fds_.emplace_back(pollfd{endpoint.fd, events, 0});
        poll_endpoints_.emplace_back(&endpoint);
    }
    posix_endpoint_list_.push_back(endpoint);
}

void UnixDomainEngine::UnregisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(IsOnCallbackThread());

    const auto found = std::find(poll_endpoints_.begin(), poll_endpoints_.end(), &endpoint);
    if (found != poll_endpoints_.end())
    {
        std::size_t i = static_cast<std::size_t>(std::distance(poll_endpoints_.begin(), found));
        UnpollEndpoint(i);
    }
}

void UnixDomainEngine::UnpollEndpoint(const std::size_t index) noexcept
{
    PosixEndpointEntry& endpoint = *poll_endpoints_[index];
    posix_endpoint_list_.erase(posix_endpoint_list_.iterator_to(endpoint));
    poll_endpoints_[index] = nullptr;
    poll_fds_[index].fd = -1;
    poll_fds_[index].revents = 0;
    if (!endpoint.disconnect.empty())
    {
        endpoint.disconnect();
    }
}

void UnixDomainEngine::EnqueueCommand(CommandQueueEntry& entry,
                                      const TimePoint until,
                                      CommandCallback callback,
                                      const void* const owner) noexcept
{
    timer_queue_.RegisterTimedEntry(entry, until, std::move(callback), owner);
    SendPipeEvent(PipeEvent::TIMER);
}

void UnixDomainEngine::CleanUpOwner(const void* const owner) noexcept
{
    if (owner == nullptr)
    {
        return;
    }
    if (IsOnCallbackThread())
    {
        ProcessCleanup(owner);
    }
    else
    {
        std::promise<void> done;  // TODO: maybe switch to NonAllocatingFuture
        detail::TimedCommandQueue::Entry cleanup_command;
        timer_queue_.RegisterImmediateEntry(
            cleanup_command,
            [this, owner, &done](auto) noexcept {
                ProcessCleanup(owner);
                done.set_value();
            },
            owner);
        SendPipeEvent(PipeEvent::TIMER);
        done.get_future().wait();
    }
}

score::cpp::expected_blank<score::os::Error> UnixDomainEngine::SendProtocolMessage(
    const std::int32_t fd,
    std::uint8_t code,
    const score::cpp::span<const std::uint8_t> message) noexcept
{
    struct msghdr msg;
    std::memset(static_cast<void*>(&msg), 0, sizeof(msg));
    constexpr auto kVectorCount = 3UL;
    std::uint16_t size = static_cast<std::uint16_t>(message.size());
    std::array<iovec, kVectorCount> io;
    io[0].iov_base = &code;
    io[0].iov_len = sizeof(code);
    io[1].iov_base = &size;
    io[1].iov_len = sizeof(size);
    io[2].iov_base = const_cast<std::uint8_t*>(message.data());
    io[2].iov_len = static_cast<std::size_t>(message.size());
    msg.msg_iov = io.data();
    msg.msg_iovlen = kVectorCount;

    const auto result_expected = os_resources_.socket->sendmsg(fd, &msg, ::score::os::Socket::MessageFlag::kWaitAll);
    if (result_expected.has_value())
    {
        return {};
    }
    return score::cpp::make_unexpected(result_expected.error());
}

score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> UnixDomainEngine::ReceiveProtocolMessage(
    const std::int32_t fd,
    std::uint8_t& code) noexcept
{
    struct msghdr msg;
    std::memset(static_cast<void*>(&msg), 0, sizeof(msg));
    constexpr auto kVectorCount = 2UL;
    std::uint16_t size{};
    std::array<iovec, kVectorCount> io;
    io[0].iov_base = &code;
    io[0].iov_len = sizeof(code);
    io[1].iov_base = &size;
    io[1].iov_len = sizeof(size);
    msg.msg_iov = io.data();
    msg.msg_iovlen = kVectorCount;

    auto size_expected = os_resources_.socket->recvmsg(fd, &msg, ::score::os::Socket::MessageFlag::kWaitAll);
    if (!size_expected.has_value())
    {
        return score::cpp::make_unexpected(size_expected.error());
    }
    if (size_expected.value() == 0)
    {
        // other side disconnected
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
    }
    if (size == 0)
    {
        return {};
    }
    if (size > static_cast<std::uint16_t>(posix_receive_buffer_.size()))
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EMSGSIZE));
    }

    io[0].iov_base = posix_receive_buffer_.data();
    io[0].iov_len = static_cast<std::size_t>(size);
    msg.msg_iovlen = 1UL;

    size_expected = os_resources_.socket->recvmsg(fd, &msg, ::score::os::Socket::MessageFlag::kWaitAll);
    if (!size_expected.has_value())
    {
        return score::cpp::make_unexpected(size_expected.error());
    }
    if (size_expected.value() != size)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EIO));
    }
    return score::cpp::span<const std::uint8_t>{posix_receive_buffer_.data(), size};
}

void UnixDomainEngine::SendPipeEvent(PipeEvent pipe_event) noexcept
{
    os_resources_.unistd->write(pipe_fds_[1], &pipe_event, sizeof(pipe_event));
}

void UnixDomainEngine::ProcessPipeEvent() noexcept
{
    PipeEvent pipe_event;
    os_resources_.unistd->read(pipe_fds_[0], &pipe_event, sizeof(pipe_event));
    if (pipe_event == PipeEvent::TIMER)
    {
        // Intentionally empty. Just wake up to recalculate poll timeout.
    }
    else
    {
        quit_flag_ = true;
    }
}

void UnixDomainEngine::ProcessCleanup(const void* const owner) noexcept
{
    for (std::size_t i = 0; i < poll_fds_.size(); ++i)
    {
        if (poll_fds_[i].fd == -1)
        {
            continue;
        }
        if (poll_endpoints_[i]->owner == owner)
        {
            UnpollEndpoint(i);
        }
    }
    timer_queue_.CleanUpOwner(owner);
}

void UnixDomainEngine::RunOnThread() noexcept
{
    command_endpoint_.owner = this;
    command_endpoint_.fd = pipe_fds_[0];
    command_endpoint_.input = [this]() noexcept {
        ProcessPipeEvent();
    };
    command_endpoint_.output = {};
    command_endpoint_.disconnect = {};
    RegisterPosixEndpoint(command_endpoint_);

    while (!quit_flag_)
    {
        std::int32_t timeout = ProcessTimerQueue();
        const auto num_expected = os_resources_.poll->poll(poll_fds_.data(), poll_fds_.size(), timeout);
        if (num_expected.has_value() && num_expected.value() > 0)
        {
            for (std::size_t i = 0; i < poll_fds_.size(); ++i)
            {
                if (poll_fds_[i].revents != 0)
                {
                    PosixEndpointEntry& endpoint = *poll_endpoints_[i];
                    endpoint.input();
                }
            }
        }
    }

    UnregisterPosixEndpoint(command_endpoint_);
}

std::int32_t UnixDomainEngine::ProcessTimerQueue() noexcept
{
    const auto now = Clock::now();
    const auto then = timer_queue_.ProcessQueue(now);
    if (then == TimePoint{})
    {
        return -1;
    }
    const auto distance = std::chrono::duration_cast<std::chrono::milliseconds>(then - Clock::now()).count() + 1;
    if (distance > INT32_MAX)
    {
        return INT32_MAX;
    }
    return static_cast<std::int32_t>(distance);
}

}  // namespace message_passing
}  // namespace score
