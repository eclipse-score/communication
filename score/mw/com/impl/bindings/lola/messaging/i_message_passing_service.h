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
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"

#include "score/result/result.h"

#include <score/callback.hpp>
#include <score/stop_token.hpp>

#include <cstddef>
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

    /// \brief Callback type to be invoked when either the first registration of an event-update-notification
    ///        occurs for the given event_id or the last event-update-notification has been withdrawn.
    /// \details The callback receives true when at least one handler is registered (transition from 0 to >0),
    ///          and false when no handlers are registered (transition from >0 to 0).
    ///          The callback is invoked synchronously during handler registration/unregistration.
    using HandlerStatusChangeCallback = score::cpp::callback<void(bool)>;

    using ServiceMethodSubscribedHandler =
        score::cpp::callback<score::ResultBlank(ProxyInstanceIdentifier proxy_instance_identifier)>;
    using MethodCallHandler = score::cpp::callback<void(std::size_t queue_position)>;

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

    /// \brief Register a handler on Skeleton side which will be called when SubscribeServiceMethod is called by a
    /// Proxy.
    ///
    /// When a Proxy is created, it will create a method shared memory region and perform some setup steps. It will then
    /// send a notification to the connected Skeleton via SubscribeServiceMethod. When this message is received in
    /// the Skeleton process, the handler registered in this function will be called.
    ///
    /// Each Skeleton containing at least one method must register a handler with this function. Since we have one
    /// IMessagePassingService per process, the incoming message must identify which Skeleton's handler should be
    /// called.
    ///
    /// \param skeleton_instance_identifier to identify which ServiceMethodSubscribedHandler to call when
    /// SubscribeServiceMethod is called on the Proxy side
    /// \param subscribed_callback callback that will be called when SubscribeServiceMethod is called
    virtual ResultBlank RegisterOnServiceMethodSubscribedHandler(
        SkeletonInstanceIdentifier skeleton_instance_identifier,
        ServiceMethodSubscribedHandler subscribed_callback) = 0;

    /// \brief Register a handler on Skeleton side which will be called when CallMethod is called by a Proxy.
    ///
    /// When a user calls a method on a Proxy, it will put the InArgs in shared memory (if there are any) and then send
    /// a notification to the Skeleton via CallMethod. The registered MethodCallCallback in the Skeleton process will
    /// then be called which calls the actual method and puts the return value in shared memory (if there is one).
    ///
    /// A Skeleton opens a shared memory region for each connected Proxy which contains a method. The provided
    /// ProxyInstanceIdentifier is required to identify which of the connected proxies the provided callback corresponds
    /// to.
    ///
    /// \param proxy_instance_identifier to identify which MethodCallHandler to call when CallMethod is called on the
    /// Proxy side
    /// \param method_call_callback callback that will be called when CallMethod is called
    virtual ResultBlank RegisterMethodCallHandler(ProxyInstanceIdentifier proxy_instance_identifier,
                                                  MethodCallHandler method_call_callback) = 0;

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

    /// \brief Registers a callback for event notification existence changes.
    /// \details This callback is invoked when the existence of event notification registrations changes:
    ///          with 'true' when the first event notification is registered and with 'false' when the last event
    ///          notification is unregistered. This allows SkeletonEvent to optimise performance by skipping
    ///          NotifyEvent() calls when no event notifications are registered. The callback is invoked synchronously
    ///          during event notification registration/unregistration. If event notifications are already registered
    ///          when this method is called, the callback is invoked immediately with 'true'.
    /// \param asil_level ASIL level of event.
    /// \param event_id The event to monitor for event notification existence changes.
    /// \param callback The callback to invoke when event notification existence changes.
    virtual void RegisterEventNotificationExistenceChangedCallback(const QualityType asil_level,
                                                                   const ElementFqId event_id,
                                                                   HandlerStatusChangeCallback callback) noexcept = 0;

    /// \brief Unregisters the callback for event notification existence changes.
    /// \details After unregistration, no further callbacks will be invoked for event notification
    ///          existence changes of this event. It is safe to call this method even if no callback
    ///          is currently registered.
    /// \param asil_level ASIL level of event.
    /// \param event_id The event to stop monitoring.
    virtual void UnregisterEventNotificationExistenceChangedCallback(const QualityType asil_level,
                                                                     const ElementFqId event_id) noexcept = 0;

    /// \brief Blocking call which is called on Proxy side to notify the Skeleton that a Proxy has setup the
    /// method shared memory region and wants to subscribe. The callback registered with RegisterMethodCall will be
    /// called on the Skeleton side and a response will be returned.
    ///
    /// The provided SkeletonInstanceIdentifier is required so that MessagePassingService can find the correct
    /// ServiceMethodSubscribed handler corresponding to the correct Skeleton.
    ///
    /// \param skeleton_instance_identifier identification of the Skeleton corresponding to the Proxy which is calling
    /// this method.
    virtual ResultBlank SubscribeServiceMethod(const SkeletonInstanceIdentifier& skeleton_instance_identifier) = 0;

    /// \brief Blocking call which is called on Proxy side to trigger the Skeleton to process a method call. The
    /// callback registered with RegisterOnServiceMethodSubscribed will be called on the Skeleton side and a response
    /// will be returned
    ///
    /// A Skeleton opens a shared memory region for each connected Proxy which contains a method. The provided
    /// ProxyInstanceIdentifier is required to identify which of the connected proxies has called the method.
    ///
    /// \param proxy_instance_identifier identification of the specific Proxy which is calling this method.
    virtual ResultBlank CallMethod(const ProxyInstanceIdentifier& proxy_instance_identifier,
                                   std::size_t queue_position) = 0;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGSERVICE_H
