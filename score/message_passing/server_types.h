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
#ifndef SCORE_LIB_MESSAGE_PASSING_SERVER_TYPES_H
#define SCORE_LIB_MESSAGE_PASSING_SERVER_TYPES_H

#include "score/message_passing/i_connection_handler.h"

#include <score/callback.hpp>
#include <score/memory.hpp>

#include <variant>

namespace score
{
namespace message_passing
{

using UserData = std::variant<void*, std::uintptr_t, score::cpp::pmr::unique_ptr<IConnectionHandler>>;

using ConnectCallback = score::cpp::callback<score::cpp::expected<UserData, score::os::Error>(
    IServerConnection& connection) /* noexcept */>;

using DisconnectCallback = score::cpp::callback<void(IServerConnection& connection) /* noexcept */>;

using MessageCallback = score::cpp::callback<score::cpp::expected_blank<score::os::Error>(
    IServerConnection& connection,
    score::cpp::span<const std::uint8_t> message) /* noexcept */>;

// Suppress "AUTOSAR C++14 A9-6-1" rule findings. This rule declares: "Data types used for interfacing with hardware
// or conforming to communication protocols shall be trivial, standard-layout and only contain members of types with
// defined sizes."
// False positive. These are OS API types.
struct ClientIdentity
{
    // coverity[autosar_cpp14_a9_6_1_violation : FALSE]
    pid_t pid;
    // coverity[autosar_cpp14_a9_6_1_violation : FALSE]
    uid_t uid;
    // coverity[autosar_cpp14_a9_6_1_violation : FALSE]
    gid_t gid;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_SERVER_TYPES_H
