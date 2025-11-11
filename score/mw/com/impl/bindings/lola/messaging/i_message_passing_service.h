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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGSERVICE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGSERVICE_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"

#include <cstdint>
#include <memory>

namespace score::mw::com::impl::lola
{

/// \brief Interface for message-based communication between LoLa proxy/skeleton instances of different processes.
/// \details This interface provides functionalities directly used by LoLa skeletons/proxies to subscribe to events
///          and register for/notify for event updates.
class IMessagePassingService
{
  public:
    using HandlerRegistrationNoType = std::uint32_t;

    IMessagePassingService() noexcept = default;

    virtual ~IMessagePassingService() noexcept = default;

    IMessagePassingService(IMessagePassingService&&) = delete;
    IMessagePassingService& operator=(IMessagePassingService&&) = delete;
    IMessagePassingService(const IMessagePassingService&) = delete;
    IMessagePassingService& operator=(const IMessagePassingService&) = delete;

    /// \brief Notification, that the given _event_id_ has been updated.
    /// \details This API is used by LoLa skeleton-event to notify all proxies, which have registered a notification
    ///          handler/callback for this _event_id_.
    /// \param asil_level asil level of event.
    /// \param event_id
    virtual void NotifyEvent(const QualityType asil_level, const ElementFqId event_id) noexcept = 0;

    /// \brief Registers a callback for event update notifications for event _event_id_
    /// \details This API is used by LoLa proxy-events in case the user has registered a receive-handler for this event.
    ///          Anytime the skeleton-event side did notify an event update (see NotifyEvent()), the registered callback
    ///          gets called.
    /// \param asil_level asil level of the provided event.
    /// \param event_id full qualified event id
    /// \param callback handler to be called, when event gets updated
    /// \param target_node_id node_id, where the event provider is located.
    /// \return a registration number, which can be used to un-register the callback again.
    virtual HandlerRegistrationNoType RegisterEventNotification(const QualityType asil_level,
                                                                const ElementFqId event_id,
                                                                std::weak_ptr<ScopedEventReceiveHandler> callback,
                                                                const pid_t target_node_id) noexcept = 0;

    /// \brief Re-registers an event update notification handler for event _event_id_ in case target_node_id is a remote
    ///        pid.
    /// \details In case the service (event) provider side identified by target_node_id has been restarted since the
    ///          last call to RegisterEventNotification(), the registration is lost. I.e. after restart of the provider
    ///          it won't send any event update notifications to our node. This API re-triggers/registers the other
    ///          side, to continue doing these notifications. The caller doesn't have to give a callback again, since
    ///          IMessagePassingService has still stored the user callback given in the original
    ///          RegisterEventNotification() call.
    /// \attention The method doesn't check, whether really a previous callback has been registered. Reason: there can
    ///            be multiple callbacks being registered for the same event_id as there could be multiple local proxy
    ///            instances, which registered an event notification callback for the same event_id. So we can't
    ///            distinguish, which callback belongs to which caller. But in case there is NO callback at all
    ///            registered for the given event_id, then an error-log will be written.
    /// \param asil_level asil level of the provided event.
    /// \param event_id full qualified event id
    /// \param target_node_id node_id, where the event provider is located.
    virtual void ReregisterEventNotification(const QualityType asil_level,
                                             const ElementFqId event_id,
                                             const pid_t target_node_id) noexcept = 0;

    /// \brief Unregister an event update notification callback, which has been registered with
    /// RegisterEventNotification()
    /// \param asil_level asil level of the provided event.
    /// \param event_id full qualified event id
    /// \param registration_no registration number of handler to be un-registered
    /// \param target_node_id node_id, where the event provider is located.
    virtual void UnregisterEventNotification(const QualityType asil_level,
                                             const ElementFqId event_id,
                                             const HandlerRegistrationNoType registration_no,
                                             const pid_t target_node_id) noexcept = 0;

    /// \brief Notify given target_node_id about outdated_node_id being an old/not to be used node identifier.
    /// \details This is used by LoLa proxy instances during creation, when they detect, that they are re-starting
    ///          (regularly or after crash) and are re-using a certain service instance, which they had used before, but
    ///          with a different node id (pid) than before. They send this notification with their previous pid to the
    ///          node, that is providing the service, so that this node can cleanup artifacts relating to the
    ///          old/previous node id. These artifacts to cleanup are EventUpdateNotificationHandlers
    ///          (EventReceiveHandlers), which have been registered previously by the LoLa proxy instance. Normally the
    ///          LoLa proxy instance would withdraw this registration on destruction. This notification serves the job
    ///          to do this cleanup, if the LoLa proxy instance crashed and didn't do the withdrawal.
    /// \param asil_level asil level of the provided event.
    /// \param outdated_node_id
    /// \param target_node_id
    virtual void NotifyOutdatedNodeId(const QualityType asil_level,
                                      const pid_t outdated_node_id,
                                      const pid_t target_node_id) noexcept = 0;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGSERVICE_H
