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
#ifndef SCORE_LIB_MESSAGE_PASSING_I_SERVER_CONNECTION_H
#define SCORE_LIB_MESSAGE_PASSING_I_SERVER_CONNECTION_H

#include "score/message_passing/server_types.h"

#include "score/os/errno.h"

#include <score/expected.hpp>
#include <score/span.hpp>

namespace score
{
namespace message_passing
{

/// \brief Interface of a Message Passing Server connection
/// \details The interface is providing the server side of asynchronous client-server IPC communication.
class IServerConnection
{
  public:
    virtual const ClientIdentity& GetClientIdentity() const noexcept = 0;
    virtual UserData& GetUserData() noexcept = 0;

    virtual score::cpp::expected_blank<score::os::Error> Reply(
        score::cpp::span<const std::uint8_t> message) noexcept = 0;
    virtual score::cpp::expected_blank<score::os::Error> Notify(
        score::cpp::span<const std::uint8_t> message) noexcept = 0;
    virtual void RequestDisconnect() noexcept = 0;

  protected:
    ~IServerConnection() noexcept = default;

    IServerConnection() noexcept = default;
    IServerConnection(const IServerConnection&) = delete;
    IServerConnection(IServerConnection&&) = delete;
    IServerConnection& operator=(const IServerConnection&) = delete;
    IServerConnection& operator=(IServerConnection&&) = delete;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_I_SERVER_CONNECTION_H
