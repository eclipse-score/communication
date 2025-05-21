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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_I_NOTIFY_EVENT_HANDLER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_I_NOTIFY_EVENT_HANDLER_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_common.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_element_fq_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/message_passing/i_receiver.h"

namespace score::mw::com::impl::lola
{
/// \brief Handles event-notification functionality of MessagePassingFacade.
///
/// \details Functional aspects, which MessagePassingFacade provides are split into different composites/handlers. This
///          class implements the handling of event-notification functionality:
///          - It processes (Un)RegisterEventNotification() calls from proxy instances and dispatches the notification
///            callbacks back to the proxy.
///          - It processes NotifyEvent() calls from skeleton instances.
class INotifyEventHandler
{
  public:
    INotifyEventHandler() = default;
    virtual ~INotifyEventHandler() = default;

    /// \brief Registers message received callbacks for messages handled by NotifyEventHandler at _receiver_
    /// \param asil_level asil level of given _receiver_
    /// \param receiver receiver, where to register
    virtual void RegisterMessageReceivedCallbacks(const QualityType asil_level,
                                                  message_passing::IReceiver& receiver) noexcept = 0;

    /// \brief Notify that event _event_id_ has been updated.
    ///
    /// \details This API is used by process local instances of LoLa skeleton-event in its implementation of ara::com
    /// event update functionality.
    ///
    /// \param asil_level needed/intended ASIL level. ASIL_B can only be used, if calling process is ASIL_B qualified
    ///                   and target provides service/event ASIL_B qualified.
    /// \param event_id identification of event to notify
    /// \param max_samples maximum number of event samples, which shall be used/buffered from caller perspective
    virtual void NotifyEvent(const QualityType asil_level, const ElementFqId event_id) noexcept = 0;

    /// \brief Add event update notification callback
    /// \details This API is used by process local LoLa proxy-events.
    /// \param asil_level needed/intended ASIL level. ASIL_B can only be used, if calling process is ASIL_B qualified
    ///                   and target provides service/event ASIL_B qualified.
    /// \param event_id fully qualified event id, for which event notification shall be registered
    /// \param callback callback to be registered
    /// \param target_node_id node id (pid) of providing LoLa process.
    virtual IMessagePassingService::HandlerRegistrationNoType RegisterEventNotification(
        const QualityType asil_level,
        const ElementFqId event_id,
        std::weak_ptr<ScopedEventReceiveHandler> callback,
        const pid_t target_node_id) noexcept = 0;

    /// \brief Re-registers an event update notifications for event _event_id_ in case target_node_id is a remote pid.
    /// \details see see IMessagePassingService::ReregisterEventNotification
    virtual void ReregisterEventNotification(QualityType asil_level,
                                             ElementFqId event_id,
                                             pid_t target_node_id) noexcept = 0;

    /// \brief Unregister an event update notification callback, which has been registered with
    ///        RegisterEventNotification()
    /// \param asil_level needed/intended ASIL level. ASIL_B can only be used, if calling process is ASIL_B qualified
    //                    and target provides service/event ASIL_B qualified.
    /// \param event_id fully qualified event id, for which event notification shall be unregistered
    /// \param registration_no registration handle, which has been returned by
    ///        RegisterEventNotification.
    /// \param target_node_id node id (pid) of event providing LoLa process.
    virtual void UnregisterEventNotification(const QualityType asil_level,
                                             const ElementFqId event_id,
                                             const IMessagePassingService::HandlerRegistrationNoType registration_no,
                                             const pid_t target_node_id) = 0;

    virtual void NotifyOutdatedNodeId(QualityType asil_level,
                                      pid_t outdated_node_id,
                                      pid_t target_node_id) noexcept = 0;

  protected:
    using NotifyEventUpdateMessage = ElementFqIdMessage<MessageType::kNotifyEvent>;
    using RegisterEventNotificationMessage = ElementFqIdMessage<MessageType::kRegisterEventNotifier>;
    using UnregisterEventNotificationMessage = ElementFqIdMessage<MessageType::kUnregisterEventNotifier>;

    INotifyEventHandler(const INotifyEventHandler&) = default;
    INotifyEventHandler& operator=(const INotifyEventHandler&) & = default;
    INotifyEventHandler(INotifyEventHandler&&) noexcept = default;
    INotifyEventHandler& operator=(INotifyEventHandler&&) & noexcept = default;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_I_NOTIFY_EVENT_HANDLER_H
