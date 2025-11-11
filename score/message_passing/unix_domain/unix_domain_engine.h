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
#ifndef SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_ENGINE_H
#define SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_ENGINE_H

#include <score/callback.hpp>
#include <score/memory.hpp>
#include <score/span.hpp>
#include <score/vector.hpp>

#include "score/message_passing/i_shared_resource_engine.h"
#include "score/message_passing/timed_command_queue.h"
#include "score/os/socket.h"
#include "score/os/sys_poll.h"
#include "score/os/unistd.h"

#include <mutex>
#include <string_view>
#include <thread>

#include <poll.h>

namespace score
{
namespace message_passing
{

/// \brief Class encapsulating resources needed for Unix Domain Client/Server implementation
/// \details The class provides access to the OSAL resource objects, memory resource, background thread with poll loop,
///          and timer queue. It also provides an implementation of a simple message exchange transport protocol
///          over a connected socket.
///          The class is supposed to be shared via std::shared_ptr between its consumers (client and server factories,
///          server objects, client and server connections).
///          One or more instances of this class, with separate background threads and potentially separate memory
///          resources, can co-exist in the same process, if needed.
class UnixDomainEngine final : public ISharedResourceEngine
{
  public:
    struct OsResources
    {
        score::cpp::pmr::unique_ptr<score::os::Socket> socket{};
        score::cpp::pmr::unique_ptr<score::os::SysPoll> poll{};
        score::cpp::pmr::unique_ptr<score::os::Unistd> unistd{};
    };

    UnixDomainEngine(score::cpp::pmr::memory_resource* memory_resource) noexcept;
    ~UnixDomainEngine() noexcept override;

    UnixDomainEngine(const UnixDomainEngine&) = delete;
    UnixDomainEngine& operator=(const UnixDomainEngine&) = delete;

    static OsResources GetDefaultOsResources(score::cpp::pmr::memory_resource* const memory_resource) noexcept
    {
        return {score::os::Socket::Default(memory_resource),
                score::os::SysPoll::Default(memory_resource),
                score::os::Unistd::Default(memory_resource)};
    }

    score::cpp::pmr::memory_resource* GetMemoryResource() noexcept override
    {
        return memory_resource_;
    }
    OsResources& GetOsResources() noexcept
    {
        return os_resources_;
    }

    using FinalizeOwnerCallback = score::cpp::callback<void() /* noexcept */>;

    score::cpp::expected<std::int32_t, score::os::Error> TryOpenClientConnection(
        std::string_view identifier) noexcept override;

    void CloseClientConnection(std::int32_t client_fd) noexcept override;

    void RegisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept override;
    void UnregisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept override;

    void EnqueueCommand(CommandQueueEntry& entry,
                        const TimePoint until,
                        CommandCallback callback,
                        const void* const owner = nullptr) noexcept override;

    // this call is blocking
    void CleanUpOwner(const void* const owner) noexcept override;

    score::cpp::expected_blank<score::os::Error> SendProtocolMessage(
        const std::int32_t fd,
        std::uint8_t code,
        const score::cpp::span<const std::uint8_t> message) noexcept override;
    score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> ReceiveProtocolMessage(
        const std::int32_t fd,
        std::uint8_t& code) noexcept override;

    bool IsOnCallbackThread() const noexcept override
    {
        return std::this_thread::get_id() == thread_.get_id();
    }

  private:
    enum class PipeEvent : uint8_t
    {
        QUIT,
        TIMER
    };

    struct CleanupCommand
    {
        FinalizeOwnerCallback callback;
        const void* owner;
    };

    void UnpollEndpoint(const std::size_t index) noexcept;
    void SendPipeEvent(PipeEvent pipe_event) noexcept;
    void ProcessPipeEvent() noexcept;
    void ProcessCleanup(const void* const owner) noexcept;
    void RunOnThread() noexcept;
    std::int32_t ProcessTimerQueue() noexcept;

    score::cpp::pmr::memory_resource* const memory_resource_;
    OsResources os_resources_;

    std::array<std::int32_t, 2> pipe_fds_;
    bool quit_flag_;
    std::thread thread_;
    std::mutex thread_mutex_;
    ISharedResourceEngine::PosixEndpointEntry command_endpoint_;

    score::cpp::pmr::vector<pollfd> poll_fds_;
    score::cpp::pmr::vector<PosixEndpointEntry*> poll_endpoints_;

    detail::TimedCommandQueue timer_queue_;
    score::containers::intrusive_list<PosixEndpointEntry> posix_endpoint_list_;
    score::cpp::pmr::vector<std::uint8_t> posix_receive_buffer_;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_ENGINE_H
