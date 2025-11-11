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
#ifndef SCORE_LIB_MESSAGE_PASSING_CLIENT_SERVER_COMMUNICATION_H
#define SCORE_LIB_MESSAGE_PASSING_CLIENT_SERVER_COMMUNICATION_H

#include <cstdint>

namespace score
{
namespace message_passing
{
namespace detail
{

enum class ClientToServer : std::uint8_t
{
    SEND,
    REQUEST
};

enum class ServerToClient : std::uint8_t
{
    REPLY,
    NOTIFY
};

}  // namespace detail
}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_CLIENT_SERVER_COMMUNICATION_H
