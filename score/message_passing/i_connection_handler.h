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
#ifndef SCORE_LIB_MESSAGE_PASSING_I_CONNECTION_HANDLER_H
#define SCORE_LIB_MESSAGE_PASSING_I_CONNECTION_HANDLER_H

#include "score/os/errno.h"

#include <score/expected.hpp>
#include <score/span.hpp>

namespace score
{
namespace message_passing
{

// forward declaration
class IServerConnection;

/// \brief The user object to handle server connections
class IConnectionHandler
{
  public:
    IConnectionHandler() = default;
    virtual ~IConnectionHandler() = default;

    IConnectionHandler(const IConnectionHandler&) = delete;
    IConnectionHandler(IConnectionHandler&&) = delete;
    IConnectionHandler& operator=(const IConnectionHandler&) = delete;
    IConnectionHandler& operator=(IConnectionHandler&&) = delete;

    virtual score::cpp::expected_blank<score::os::Error> OnMessageSent(
        IServerConnection& connection,
        score::cpp::span<const std::uint8_t> message) noexcept = 0;
    virtual score::cpp::expected_blank<score::os::Error> OnMessageSentWithReply(
        IServerConnection& connection,
        score::cpp::span<const std::uint8_t> message) noexcept = 0;
    virtual void OnDisconnect(IServerConnection& connection) noexcept = 0;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_I_CONNECTION_HANDLER_H
