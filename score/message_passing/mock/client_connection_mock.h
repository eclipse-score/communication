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
#ifndef SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_CONNECTION_MOCK_H
#define SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_CONNECTION_MOCK_H

#include "score/message_passing/i_client_connection.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class ClientConnectionMock : public IClientConnection
{
  public:
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, Send, (score::cpp::span<const std::uint8_t>), (noexcept, override));
    MOCK_METHOD((score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error>),
                SendWaitReply,
                (score::cpp::span<const std::uint8_t>, score::cpp::span<std::uint8_t>),
                (noexcept, override));
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>,
                SendWithCallback,
                (score::cpp::span<const std::uint8_t>, ReplyCallback),
                (noexcept, override));
    MOCK_METHOD(State, GetState, (), (const, noexcept, override));
    MOCK_METHOD(StopReason, GetStopReason, (), (const, noexcept, override));
    MOCK_METHOD(void, Start, (StateCallback, NotifyCallback), (noexcept, override));
    MOCK_METHOD(void, Stop, (), (noexcept, override));
    MOCK_METHOD(void, Restart, (), (noexcept, override));

    // mockable virtual destructor
    MOCK_METHOD(void, Destruct, (), (noexcept));
    ~ClientConnectionMock() override
    {
        Destruct();
    }
};

/// \brief Facade class which dispatches to a mock object which is owned by the caller
///
/// Such a facade class is useful when a test needs to mock an object for which ownership must be passed to the
/// class under test e.g. via a unique_ptr. In this case, the test requires that the mock survives until the end of the
/// test (or whenever it wants to evaluate the expectations on the mocks), however, this cannot be guaranteed when
/// handing ownership to the class under test. Therefore, the test can create the mock object and provide a facade
/// object to the class under test (which will dispatch any calls to the mock object).
class ClientConnectionMockFacade : public IClientConnection
{
  public:
    ClientConnectionMockFacade(ClientConnectionMock& client_connection_mock)
        : client_connection_mock_{client_connection_mock}
    {
    }
    ~ClientConnectionMockFacade() override = default;

    score::cpp::expected_blank<score::os::Error> Send(score::cpp::span<const std::uint8_t> message) noexcept override
    {
        return client_connection_mock_.Send(message);
    }

    score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> SendWaitReply(
        score::cpp::span<const std::uint8_t> message,
        score::cpp::span<std::uint8_t> reply) noexcept override
    {
        return client_connection_mock_.SendWaitReply(message, reply);
    }

    score::cpp::expected_blank<score::os::Error> SendWithCallback(score::cpp::span<const std::uint8_t> message,
                                                         ReplyCallback callback) noexcept override
    {
        return client_connection_mock_.SendWithCallback(message, std::move(callback));
    }

    State GetState() const noexcept override
    {
        return client_connection_mock_.GetState();
    }

    StopReason GetStopReason() const noexcept override
    {
        return client_connection_mock_.GetStopReason();
    }

    void Start(StateCallback state_callback, NotifyCallback notify_callback) noexcept override
    {
        client_connection_mock_.Start(std::move(state_callback), std::move(notify_callback));
    }

    void Stop() noexcept override
    {
        client_connection_mock_.Stop();
    }

    void Restart() noexcept override
    {
        client_connection_mock_.Restart();
    }

  private:
    ClientConnectionMock& client_connection_mock_;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_CONNECTION_MOCK_H
