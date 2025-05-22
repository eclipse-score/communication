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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGCONTROL_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGCONTROL_H

#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/message_passing/i_sender.h"

#include <memory>
#include <string>

namespace score::mw::com::impl::lola
{

/// \brief Interface for control of message-based communication between LoLa proxy/skeleton instances.
/// \details This interface is not used directly (opposed to IMessagePassingFacade) by proxy/skeleton instances,
///          but by implementation of IMessagePassingFacade.
class IMessagePassingControl
{
  public:
    IMessagePassingControl() noexcept = default;

    virtual ~IMessagePassingControl() noexcept = default;

    IMessagePassingControl(IMessagePassingControl&&) = delete;
    IMessagePassingControl& operator=(IMessagePassingControl&&) = delete;
    IMessagePassingControl(const IMessagePassingControl&) = delete;
    IMessagePassingControl& operator=(const IMessagePassingControl&) = delete;

    /// \brief returns existing sender for given _asil_level_ and _target_pid_ or creates one if not yet exists.
    /// \pre asil_level is verified to be either QualityType::kASIL_QM or QualityType::kASIL_B
    /// \param asil_level
    /// \param target_node_id node_id (pid) of the target, where sender shall send to
    /// \return a shared_ptr to an ISender for sending asil_level messages to target_node_id on success
    virtual std::shared_ptr<score::mw::com::message_passing::ISender> GetMessagePassingSender(
        const QualityType asil_level,
        const pid_t target_node_id) noexcept = 0;

    /// \brief Removes all (up to two in case of ASIL-B support) message passing senders for the given target node id.
    /// \param asil_level asil-level of the process, that was identified by target_node_id. We receive the trigger for
    ///                   removal via message-passing from the same application after restart. So we deduce the
    ///                   asil-level from the message-passing Receiver, where we receive this trigger.
    /// \param target_node_id identification of the target node, with which the sender to be removed "speaks"
    virtual void RemoveMessagePassingSender(const QualityType asil_level, pid_t target_node_id) = 0;

    /// \brief Creates a standardized name for message passing channel (MQ name)
    /// \param asil_level
    /// \param node_id
    /// \return string denoting the LoLa specific name for message passing.
    virtual std::string CreateMessagePassingName(const QualityType asil_level, const pid_t node_id) noexcept = 0;

    /// \brief returns the NodeIdentifier (pid) used by the (single) MessagePassingFacade instance.
    /// \return node_identifier (pid) of local node
    virtual pid_t GetNodeIdentifier() const noexcept = 0;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGCONTROL_H
