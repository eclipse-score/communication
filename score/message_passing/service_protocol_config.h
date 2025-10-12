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
#ifndef SCORE_LIB_MESSAGE_PASSING_SERVICE_PROTOCOL_CONFIG_H
#define SCORE_LIB_MESSAGE_PASSING_SERVICE_PROTOCOL_CONFIG_H

#include <cstdint>
#include <memory>
#include <string_view>

namespace score
{
namespace message_passing
{

/// \brief The common part of the library configuration of a server and a client
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init) POD-like usage
struct ServiceProtocolConfig
{
    std::string_view identifier;    ///< The server name in the service namespace
    std::uint32_t max_send_size;    ///< Maximum size in bytes for the message from client to server
    std::uint32_t max_reply_size;   ///< Maximum size in bytes for the reply from server to client
    std::uint32_t max_notify_size;  ///< Maximum size in bytes for the notification message from server to client
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_SERVICE_PROTOCOL_CONFIG_H
