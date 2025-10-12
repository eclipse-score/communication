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
#ifndef SCORE_LIB_MESSAGE_PASSING_I_CLIENT_CONNECTION_H
#define SCORE_LIB_MESSAGE_PASSING_I_CLIENT_CONNECTION_H

#include "score/os/errno.h"

#include <score/callback.hpp>
#include <score/expected.hpp>
#include <score/span.hpp>

namespace score
{
namespace message_passing
{

/// \brief Interface of a Message Passing Client connection
/// \details The interface is providing the client side of asynchronous client-server IPC communication.
class IClientConnection
{
  public:
    /// \brief The destructor can be used to free the resources associated with the client connection.
    /// \details The destructor is supposed to be non-blocking in the sense of IPC communication. It is unsafe to
    ///          destruct the connection that is not in the Stopped state. It is unsafe to destruct the connection from
    ///          any of the connection's callbacks except for the StateCallback with the Stopped state as an argument.
    virtual ~IClientConnection() = default;

    /// \brief Send a binary message to the respective server, don't expect a reply
    /// \details The call is non-blocking and will fail if it would otherwise block due to implementation details.
    ///          The call will likely fail with score::os::Error in State::Init and definitely fail in State::Stopped.
    ///          The call is unlikely (but still possible) to fail in State::Ready, unless the message is too big to
    ///          transport, in which case the call will definitely fail. Even if the call doesn't fail, there is still
    ///          a possibility that the message will be lost by the receiving side, however, that only happens if the
    ///          receiving side dies or disconnects before receiving the message.
    ///          The implementation is allowed to use different transport mechanisms depending on the message size;
    ///          the server is still guaranteed to receive the messages from the same client connection sequentially in
    ///          the same order as they were sent. However, if multiple connections are created from the same client
    ///          process, to the same or to the different servers, the order of messages across connections is not
    ///          guaranteed.
    ///          Note: the non-blocking guarantee for an ASIL B client to a QM server is only given if the messages to
    ///          send are queued on the client side (ClientConfig::maxQueuedSends is not 0).
    /// \param message The memory span containing the message to send
    /// \return error if fails
    virtual score::cpp::expected_blank<score::os::Error> Send(score::cpp::span<const std::uint8_t> message) noexcept = 0;

    /// \brief Send a binary message to the respective server, wait for reply
    /// \details The call is blocking.
    /// \param message The memory span containing the message to send
    /// \param reply The memory span designating the buffer for the reply message
    /// \return the reply span trimmed to the actual size of the reply message if succeeds, error if fails
    virtual score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> SendWaitReply(
        score::cpp::span<const std::uint8_t> message,
        score::cpp::span<std::uint8_t> reply) noexcept = 0;

    /// \brief The type for the callback to be called when a reply (or an error) from the server is received
    /// \details The callback is called on an unspecified thread.
    ///          It is allowed to call asynchronous send methods from inside the callback.
    using ReplyCallback = score::cpp::callback<void(
        score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> message_expected) /* noexcept */>;

    /// \brief Send a binary message to the respective server, register callback for reply
    /// \details The call is non-blocking.
    ///          Note: the non-blocking guarantee for an ASIL B client to a QM server is only given if the messages to
    ///          send are queued on the client side (ClientConfig::maxQueuedSends is not 0).
    ///          Note: if the server receives the message but dies or disconnects before the reply arrives to the
    ///          client, the error will be passed to the callback; the method itself will not return an error.
    /// \param message The memory span containing the message to send
    /// \param callback The callback to call on reply
    /// \return error if fails
    virtual score::cpp::expected_blank<score::os::Error> SendWithCallback(score::cpp::span<const std::uint8_t> message,
                                                                 ReplyCallback callback) noexcept = 0;

    /// \brief The type for the callback to be called when a notification from the server is received
    /// \details The callback is called on an unspecified thread.
    ///          It is allowed to call Stop() and asynchronous send methods from within this callback.
    using NotifyCallback = score::cpp::callback<void(score::cpp::span<const std::uint8_t> message) /* noexcept */>;

    /// \brief Represents the current state of the connection
    /// \details The State enum conveys the information about the current state of the connection regarding the expected
    ///          chances to deliver the message being sent. The channel behind the IClientConnection object is typically
    ///          created asynchronously, the channel is not yet ready to transmit when the instance of IClientConnection
    ///          already exists and its Start() method has already been called. That corresponds to the Starting state.
    ///          When the channel has been successfully created, this corresponds to Ready state. If connection cannot
    ///          be made due to some failure, or if the implementation detected that the connection has been terminated,
    ///          this corresponds to the state Stopped.
    ///          The state diagram:
    ///              Stopped --> Starting: Start() or Restart() has been called
    ///              Starting --> Ready: connection happened
    ///              Starting --> Stopping: connection failed to be established or Stop() has been called
    ///              Ready --> Stopping: connection dropped or Stop() has been called
    ///              Stopping --> Stopped: all background activity requiring a live connection object has finished
    enum class State : std::uint8_t
    {
        kStarting,  ///< The state in which the IClientConnection is still trying to connect to the server.
        kReady,     ///< The state in which the IClientConnection is most likely to deliver the messages sent.
        kStopping,  ///< The state in which the IClientConnection will not accept more messages or callbacks.
        kStopped    ///< The state in which the IClientConnection can be safely destructed.
    };

    /// \brief Represents the reason why the connection is in the stopping or stopped state
    enum class StopReason : std::uint8_t
    {
        kNone,           ///< The connection is not in the Stopping/Stopped state
        kInit,           ///< The connection has not been started yet
        kUserRequested,  ///< The user called the Stop() method
        kPermission,     ///< The access rights are not sufficient to connect to the server
        kClosedByPeer,   ///< The other side closed the connection
        kIoError,        ///< A communication error prevents continuation of the connection
        kShutdown        ///< The underlying resources need to be freed; restart is not possible
    };

    /// \brief The type for the callback to be called when the internal state of the connection changes
    /// \details The callback is called in the following context:
    ///              Starting: During the Start() or Restart() call in the same thread. If restart is not allowed,
    ///                        the callback will not be called. It is allowed to call Stop() from within this callback.
    ///              Ready: From an unspecified thread, but sequentially after return from Starting callback. It is
    ///                     allowed to call Stop() and asynchronous send methods from within the callback.
    ///              Stopping: If caused by a failed connection, then from an unspecified thread, but sequentially after
    ///                        return from Starting or Ready callback, depending on whether the connection was
    ///                        successfully established before the failure. If the stop is requested by the Stop() call,
    ///                        then during the Stop() call, normally in the same thread. A race condition between the
    ///                        failed connection and the Stop() call is possible and allowed; the result will be just a
    ///                        single callback with non-deterministically chosen StopReason.
    ///              Stopped: From an unspecified thread, but sequentially after return from Stopping callback. It is
    ///                       allowed to call Restart() from within the callback. Alternatively. it is allowed to
    ///                       destruct the connection object from within the callback.
    using StateCallback = score::cpp::callback<void(State) /* noexcept */>;

    /// \brief Return the current state
    virtual State GetState() const noexcept = 0;

    /// \brief Return the last stopping reason for the State::Stopping/State::Stopped
    virtual StopReason GetStopReason() const noexcept = 0;

    /// \brief Start the connection
    /// \details The callbacks lifetime (or rather the lifetime of the system state captured in the callbacks by
    ///          reference) needs to end not earlier than after the connection has returned into the Stop state
    virtual void Start(StateCallback state_callback, NotifyCallback notify_callback) noexcept = 0;

    /// \brief Stop the existing connection
    virtual void Stop() noexcept = 0;

    /// \brief Try to restart a stopped connection
    /// \details For some stop reasons (such as kPermission) it is highly unlikely to have a successful restart.
    virtual void Restart() noexcept = 0;

  protected:
    IClientConnection() noexcept = default;
    IClientConnection(const IClientConnection&) = delete;
    IClientConnection(IClientConnection&&) = delete;
    IClientConnection& operator=(const IClientConnection&) = delete;
    IClientConnection& operator=(IClientConnection&&) = delete;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_I_CLIENT_CONNECTION_H
