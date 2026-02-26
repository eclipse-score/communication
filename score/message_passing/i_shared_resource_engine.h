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
#ifndef SCORE_LIB_MESSAGE_PASSING_I_SHARED_RESOURCE_ENGINE_H
#define SCORE_LIB_MESSAGE_PASSING_I_SHARED_RESOURCE_ENGINE_H

#include <score/memory.hpp>
#include <score/span.hpp>

#include <string_view>

#include "score/os/errno.h"

#include "score/message_passing/log/logging_callback.h"
#include "score/message_passing/timed_command_queue_entry.h"

namespace score
{
namespace message_passing
{

class ISharedResourceEngine
{
  public:
    virtual ~ISharedResourceEngine() noexcept = default;

    virtual score::cpp::pmr::memory_resource* GetMemoryResource() noexcept = 0;

    virtual const LoggingCallback& GetLogger() noexcept = 0;

    virtual bool IsOnCallbackThread() const noexcept = 0;

    virtual score::cpp::expected<std::int32_t, score::os::Error> TryOpenClientConnection(
        std::string_view identifier) noexcept = 0;

    virtual void CloseClientConnection(std::int32_t client_fd) noexcept = 0;

    virtual score::cpp::expected_blank<score::os::Error> SendProtocolMessage(
        const std::int32_t fd,
        std::uint8_t code,
        const score::cpp::span<const std::uint8_t> message) noexcept = 0;
    virtual score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> ReceiveProtocolMessage(
        const std::int32_t fd,
        std::uint8_t& code) noexcept = 0;

    using Clock = detail::TimedCommandQueueEntry::Clock;
    using TimePoint = detail::TimedCommandQueueEntry::TimePoint;
    using CommandCallback = detail::TimedCommandQueueEntry::QueuedCallback;
    using CommandQueueEntry = detail::TimedCommandQueueEntry;

    static TimePoint FromNow(const Clock::duration duration)
    {
        return Clock::now() + duration;
    }

    virtual void EnqueueCommand(CommandQueueEntry& entry,
                                const TimePoint until,
                                CommandCallback callback,
                                const void* const owner) noexcept = 0;

    using EndpointCallback = score::cpp::callback<void(void) /* noexcept */>;
    class PosixEndpointEntry : public score::containers::intrusive_list_element<>
    {
      public:
        PosixEndpointEntry() : score::containers::intrusive_list_element<>{} {}
        void* owner{nullptr};
        std::int32_t fd{-1};
        std::uint32_t max_receive_size{0};
        EndpointCallback input{};       // Called when fd is ready to read
        EndpointCallback output{};      // TODO: reserved for future use
        EndpointCallback disconnect{};  // called when endpoint is deactivated by UnregisterPosixEndpoint()
                                        // or CleanUpOwner()
    };
    virtual void RegisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept = 0;
    virtual void UnregisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept = 0;

    // this call is blocking
    virtual void CleanUpOwner(const void* const owner) noexcept = 0;

  protected:
    ISharedResourceEngine() noexcept = default;
    ISharedResourceEngine(const ISharedResourceEngine&) = delete;
    ISharedResourceEngine(ISharedResourceEngine&&) = delete;
    ISharedResourceEngine& operator=(const ISharedResourceEngine&) = delete;
    ISharedResourceEngine& operator=(ISharedResourceEngine&&) = delete;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_I_SHARED_RESOURCE_ENGINE_H
