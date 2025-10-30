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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_I_RECEIVER_H
#define SCORE_MW_COM_MESSAGE_PASSING_I_RECEIVER_H

#include "score/mw/com/message_passing/shared_properties.h"
#include "score/os/errno.h"

#include <score/callback.hpp>
#include <score/expected.hpp>

namespace score::mw::com::message_passing
{

/// \brief Interface of a Message Passing Receiver, which can be used to receive Messages from a uni-directional
///        channel.
/// \details IReceiver foresees overloads for Register() for differently sized messages. For further explanation about
///          message size overloads check explanation in ISender.
/// \see ISender
class IReceiver
{
  public:
    using ShortMessageReceivedCallback = score::cpp::callback<void(const ShortMessagePayload, const pid_t)>;
    using MediumMessageReceivedCallback = score::cpp::callback<void(const MediumMessagePayload, const pid_t)>;

    virtual ~IReceiver() = default;

    IReceiver() = default;
    IReceiver(const IReceiver&) = delete;
    IReceiver(IReceiver&&) noexcept = delete;
    IReceiver& operator=(const IReceiver&) = delete;
    IReceiver& operator=(IReceiver&&) noexcept = delete;

    /// \brief Registers short messages within the Receiver for reception
    /// \param id The ID of the message, once received respective callback will be invoked. IDs must be unique across
    ///        short and medium messages.
    /// \param callback The callback of the message which will be invoked
    /// \pre Must not be called after StartListing() has been invoked (thread-race!)
    virtual void Register(const MessageId id, ShortMessageReceivedCallback callback) noexcept = 0;

    /// \brief Registers medium sized messages within the Receiver for reception
    /// \param id The ID of the message, once received respective callback will be invoked. IDs must be unique across
    //         short and medium messages.
    /// \param callback The callback of the message which will be invoked
    /// \pre Must not be called after StartListing() has been invoked (thread-race!)
    virtual void Register(const MessageId id, MediumMessageReceivedCallback callback) noexcept = 0;

    /// \brief Opens the underlying communication channel and listens for messages
    /// \post Must not call Register() afterwards! (thread-race)
    virtual score::cpp::expected_blank<score::os::Error> StartListening() noexcept = 0;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_I_RECEIVER_H
