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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGE_ELEMENT_FQ_ID_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGE_ELEMENT_FQ_ID_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_common.h"
#include "score/mw/com/message_passing/message.h"
#include "score/os/unistd.h"

namespace score::mw::com::impl::lola
{
/// \brief This template class serves for messages, which only contain a ElementFqId as payload. We have a lot of such
///        messages, therefore the introduction of this class template.
template <MessageType id>
class ElementFqIdMessage
{
    static constexpr message_passing::MessageId kMessageId = static_cast<message_passing::MessageId>(id);

  public:
    static ElementFqIdMessage DeserializeToElementFqIdMessage(
        const message_passing::ShortMessagePayload message_payload,
        const pid_t sender_node_id)
    {
        const ElementFqId element_fq_id = ShortMsgPayloadToElementFqId(message_payload);
        return {element_fq_id, sender_node_id};
    }

    /// \brief ctor to create ElementFqIdMessage from its members (used on sender side).
    /// \param element_fq_id
    /// \param sender_node_id node id of sender of this message
    ElementFqIdMessage(ElementFqId element_fq_id, pid_t sender_node_id) noexcept
        : element_fq_id_{element_fq_id}, sender_node_id_{sender_node_id} {};

    /// \brief Serializes message to a ShortMessage.
    /// \return ShortMessage representation
    message_passing::ShortMessage SerializeToShortMessage() const noexcept
    {
        message_passing::ShortMessage message{};
        message.id = kMessageId;
        message.pid = sender_node_id_;
        message.payload = ElementFqIdToShortMsgPayload(element_fq_id_);
        return message;
    }

    /// \brief Provide an access to the private member element fq id.
    /// \return ElementFqId
    ElementFqId GetElementFqId() const noexcept
    {
        return element_fq_id_;
    }

    /// \brief Provide an access to the private member sender node id.
    /// \return pid_t
    pid_t GetSenderNodeId() const noexcept
    {
        return sender_node_id_;
    }

  private:
    ElementFqId element_fq_id_;
    pid_t sender_node_id_;
};

template <MessageType id>
bool operator==(const ElementFqIdMessage<id>& lhs, const ElementFqIdMessage<id>& rhs) noexcept
{
    return ((lhs.GetElementFqId() == rhs.GetElementFqId()) && (lhs.GetSenderNodeId() == lhs.GetSenderNodeId()));
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGE_ELEMENT_FQ_ID_H
