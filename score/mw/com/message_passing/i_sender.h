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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_I_SENDER_H
#define SCORE_MW_COM_MESSAGE_PASSING_I_SENDER_H

#include "score/mw/com/message_passing/message.h"
#include "score/mw/com/message_passing/shared_properties.h"
#include "score/os/errno.h"

#include <score/expected.hpp>

namespace score::mw::com::message_passing
{

/// \brief Interface of a Message Passing Sender, which can be used to send Message in an uni-directional channel.
/// \details ISender foresees overloads for Send() for differently sized messages. The reason for this is, that some OS
///          might provide more efficient IPC mechanisms for very short messages. So OS/IPC specific implementations of
///          ISender shall use the best performing mechanism. Additionally, the messages (see BaseMessage) also contain
///          some meta-info besides the payload (f.i. PID of the sender, message type/id). Some OS IPC mechanisms
///          (hint: QNX messaging) provide a separate "channel" to transfer such meta-data, So instead of adding it to
///          the transferred message payload, implementations of ISender are encouraged to use such features as it might
///          keep the effective data to be transferred small, which helps performance.
class ISender
{
  public:
    virtual ~ISender() = default;

    ISender() = default;
    ISender(const ISender&) = delete;
    ISender(ISender&&) noexcept = delete;
    ISender& operator=(const ISender&) = delete;
    ISender& operator=(ISender&&) noexcept = delete;

    /// \brief Send given _message_ (short type) to the respective Receiver (using the OS most performant mechanism)
    /// \param message The short size message to send
    virtual score::cpp::expected_blank<score::os::Error> Send(const ShortMessage& message) noexcept = 0;
    /// \brief Send given _message_ (medium type) to the respective Receiver (using the OS most performant mechanism)
    /// \param message The medium size message to send
    virtual score::cpp::expected_blank<score::os::Error> Send(const MediumMessage& message) noexcept = 0;
    /// \brief Returns, whether this sender guarantees to never block on a Send() call under whatever circumstances or
    ///        not.
    /// \details  ISender/IReceiver contract is generally asynchronous. That means, any implementation of ISender/
    ///           IReceiver needs to assure a decoupling of a message sent by ISender and the processing of
    ///           the message by IReceiver. I.e. any implementation, which unblocks the ISender from its Send() call
    ///           only after the IReceiver has processed the message would be an invalid implementation as it violates
    ///           our async contract!
    ///           For some safety use cases, we need even a higher warranty than just a basic async implementation. In
    ///           case we have an application with a higher ASIL, which uses ISender to send a message to an application
    ///           with a lower ASIL, we need to technically guarantee, that the sending app gets never blocked, even if
    ///           on the receiver side (or at the transmission channel itself, which is typically provided by the OS)
    ///           any unexpected failure happens!
    /// \return true only in case it is warranted, that the Send() call cen never block, false else.
    virtual bool HasNonBlockingGuarantee() const noexcept = 0;
};

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_I_SENDER_H
