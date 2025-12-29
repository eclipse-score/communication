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
#ifndef SCORE_LIB_MESSAGE_PASSING_MOCK_SHARED_RESOURCE_ENGINE_MOCK_H
#define SCORE_LIB_MESSAGE_PASSING_MOCK_SHARED_RESOURCE_ENGINE_MOCK_H

#include "score/message_passing/i_shared_resource_engine.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class SharedResourceEngineMock : public ISharedResourceEngine
{
  public:
    MOCK_METHOD(score::cpp::pmr::memory_resource*, GetMemoryResource, (), (noexcept, override));
    MOCK_METHOD(LoggingCallback&, GetLogger, (), (noexcept, override));
    MOCK_METHOD(bool, IsOnCallbackThread, (), (const, noexcept, override));
    MOCK_METHOD((score::cpp::expected<std::int32_t, score::os::Error>),
                TryOpenClientConnection,
                (std::string_view identifier),
                (noexcept, override));
    MOCK_METHOD(void, CloseClientConnection, (std::int32_t client_fd), (noexcept, override));
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>,
                SendProtocolMessage,
                (const std::int32_t fd, std::uint8_t code, const score::cpp::span<const std::uint8_t> message),
                (noexcept, override));
    MOCK_METHOD((score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error>),
                ReceiveProtocolMessage,
                (const std::int32_t fd, std::uint8_t& code),
                (noexcept, override));
    MOCK_METHOD(void,
                EnqueueCommand,
                (CommandQueueEntry & entry, const TimePoint until, CommandCallback callback, const void* const owner),
                (noexcept, override));

    MOCK_METHOD(void, RegisterPosixEndpoint, (PosixEndpointEntry & endpoint), (noexcept, override));
    MOCK_METHOD(void, UnregisterPosixEndpoint, (PosixEndpointEntry & endpoint), (noexcept, override));
    MOCK_METHOD(void, CleanUpOwner, (const void* const owner), (noexcept, override));
};

}  // namespace message_passing
}  // namespace score
#endif  // SCORE_LIB_MESSAGE_PASSING_MOCK_SHARED_RESOURCE_ENGINE_MOCK_H
