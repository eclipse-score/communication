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
#ifndef SCORE_LIB_MESSAGE_PASSING_I_SERVER_H
#define SCORE_LIB_MESSAGE_PASSING_I_SERVER_H

#include "score/message_passing/server_types.h"

#include "score/os/errno.h"

namespace score
{
namespace message_passing
{

/// \brief Interface of a Message Passing Server.
/// \details The interface is providing the server side of asynchronous client-server IPC communication.
///          One server can communicate with multiple clients, each in its own associated session. Multiple
///          sessions from multiple clients per one client process (pid) toward the same server are possible.
///          The callbacks are called on unspecified threads, but it's guaranteed that all the callbacks belonging to
///          the same session are serialized.
class IServer
{
  public:
    virtual ~IServer() = default;

    /// \brief Sets up the callbacks for connection, disconnection and message reception notifications.
    /// \details The callbacks lifetime (or rather the lifetime of the system state captured in the callbacks by
    ///          reference) needs to end not earlier than after StopListening() (or the destructor) has returned.
    ///          The callbacks other than the ConnectCallback are only used for the connections where UserData
    ///          is not an instance of score::cpp::pmr::unique_ptr<IConnectionHandler>.
    ///          For score::cpp::pmr::unique_ptr<IConnectionHandler> connection, the corresponding IConnectionHandler's
    ///          callback methods are used instead.
    // NOLINTNEXTLINE(google-default-arguments) TODO:
    virtual score::cpp::expected_blank<score::os::Error> StartListening(
        ConnectCallback connect_callback,
        DisconnectCallback disconnect_callback = DisconnectCallback{},
        MessageCallback sent_callback = MessageCallback{},
        MessageCallback sent_with_reply_callback = MessageCallback{}) noexcept = 0;
    /// \brief Releases the callbacks abd closes all the still running server connections.
    /// \details The resources associated with the callbacks can be released/reused after the function returns.
    ///          The function may block until a currently running callback finishes, so it shall not be called from
    ///          any of the server callbacks.
    virtual void StopListening() noexcept = 0;

  protected:
    IServer() noexcept = default;
    IServer(const IServer&) = delete;
    IServer(IServer&&) = delete;
    IServer& operator=(const IServer&) = delete;
    IServer& operator=(IServer&&) = delete;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_I_SERVER_H
