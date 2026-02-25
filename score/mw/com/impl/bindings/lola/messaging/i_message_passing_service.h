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
#include "score/mw/com/impl/bindings/lola/messaging/method_call_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/messaging/method_subscription_registration_guard.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"

#include "score/language/safecpp/scoped_function/copyable_scoped_function.h"

#include "score/result/result.h"

#include <score/callback.hpp>
#include <score/stop_token.hpp>

#include <sched.h>
#include <sys/types.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>

namespace score::mw::com::impl::lola
{

/// \brief Interface for message-based communication between LoLa proxy/skeleton instances of different processes.
/// \details This interface provides functionalities directly used by LoLa skeletons/proxies to subscribe to events
///          and register for/notify for event updates.
class IMessagePassingService
{
    friend class MethodSubscriptionRegistrationGuardFactory;
    friend class MethodCallRegistrationGuardFactory;

  public:
    using HandlerRegistrationNoType = std::uint32_t;

    /// \brief Callback type to be invoked when either the first registration of an event-update-notification
    ///        occurs for the given event_id or the last event-update-notification has been withdrawn.
    /// \details The callback receives true when at least one handler is registered (transition from 0 to >0),
    ///          and false when no handlers are registered (transition from >0 to 0).
    ///          The callback is invoked synchronously during handler registration/unregistration.
    using HandlerStatusChangeCallback = score::cpp::callback<void(bool)>;

    /// \brief Handler which will be called when the proxy process sends a message that it has subscribed to a service
    /// method.
    ///
    /// On creation, a proxy will create the methods shared memory region and then call SubscribeServiceMethod
    /// which will send a message to the Skeleton process. The Skeleton process will then call the
    /// ServiceMethodSubscribedHandler registered in RegisterOnServiceMethodSubscribedHandler. This message should
    /// contain the ProxyInstanceIdentifier which will be used to call this handler. The proxy_uid and is_method_asil_b
    /// should be retrieved from the message meta data.
    ///
    /// The handler is a CopyableScopedFunction since it will be stored in a map which can be read-from and written-to
    /// concurrently. When a handler needs to be called, it will be copied out of the map under lock and called without
    /// locking. See the docstring for MethodCallHandler below for further details.
    ///
    /// \param proxy_instance_identifier unique identifier of the Proxy instance which subscribed to the method so that
    ///        the Skeleton can identify which methods shared memory region to open.
    /// \param proxy_uid UID of the Proxy instance which subscribed to the method. The Skeleton will check that this UID
    ///        exists in the allowed_consumer list in the configuration (it's allowed_consumer since this is normally
    ///        referring to consumers of the event/field shared memory region that the Skeleton creates. However, for
    ///        methods, the proxy whose UID must be checked is actually the provider of the shared memory region). It's
    ///        also used in the allowed_provider list when opening the shared memory region.
    /// \param proxy_pid PID of the proxy process which is used to determine whether the call to
    ///        SubscribeServiceMethod by a proxy is being called after the proxy process restarted and recreated
    ///        the methods shared memory region. If it is being called after a restart, then the Skeleton needs to open
    ///        the new shared memory region and close all methods shared memory regions that were in the process that
    ///        restarted.
    using ServiceMethodSubscribedHandler = safecpp::CopyableScopedFunction<
        score::ResultBlank(ProxyInstanceIdentifier proxy_instance_identifier, uid_t proxy_uid, pid_t proxy_pid)>;

    /// \brief Handler which will be called when the proxy process sends a message that it has called a method.
    ///
    /// This will be triggered when the Proxy process calls CallMethod.
    ///
    /// The handler is a CopyableScopedFunction since it will be stored in a map which can be read-from and
    /// written-to concurrently. When a handler needs to be called, it will be copied out of the map under lock and
    /// called without locking. This ensures that another thread can then remove the handler from the map under lock
    /// without having to wait for the method call to finish. This would be required when a Proxy restarts and the
    /// Skeleton must replace the existing method call handler with a new one (which works with the new shared
    /// memory region which was created by the restarted Proxy). We use a CopyableScopedFunction rather than moving
    /// the handler out of the map under lock, calling it outside the lock and moving it back in under lock to
    /// reduce the number of mutex locks and also avoid potential reallocations when inserting it back into the map.
    /// Only one copy of the handler will ever be called at one time, so the provided handler does not need to
    /// ensure it can safely be called concurrently.
    using MethodCallHandler = safecpp::CopyableScopedFunction<void(std::size_t queue_position)>;

    /// \brief Allowed consumer uids which define which processes can subscribe to and call service methods.
    ///
    /// If the optional is empty, is indicates that any uid is allowed.
    /// If the optional is filled, then all allowed uids are listed in the set. An empty set indicates that no uids are
    /// allowed.
    using AllowedConsumerUids = std::optional<std::set<uid_t>>;

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
    /// \param asil_level ASIL level of method.
    /// \param skeleton_instance_identifier to identify which ServiceMethodSubscribedHandler to call when
    ///        SubscribeServiceMethod is called on the Proxy side
    /// \param subscribed_callback callback that will be called when SubscribeServiceMethod is called
    /// \param allowed_proxy_uids set of uids of processes containing proxies which are allowed to register to this
    ///        method. This will be derived from the allowed_consumer set in the mw/com configuration using the
    ///        asil_level provided to this function (i.e. the ASIL-level of the particular MessagePassingServiceInstance
    ///        on which we'll register the handler). If an empty optional is provided, it means all uids are allowed. A
    ///        set of uids is provided because different Proxy instances will be able to subscribe via this registered
    ///        handler.
    virtual Result<MethodSubscriptionRegistrationGuard> RegisterOnServiceMethodSubscribedHandler(
        const QualityType asil_level,
        const SkeletonInstanceIdentifier skeleton_instance_identifier,
        ServiceMethodSubscribedHandler subscribed_callback,
        AllowedConsumerUids allowed_proxy_uids) = 0;

    /// \brief Register a handler on Skeleton side which will be called when CallMethod is called by a ProxyMethod.
    ///
    /// When a user calls a method on a ProxyMethod, it will put the InArgs in shared memory (if there are any) and then
    /// send a notification to the Skeleton via CallMethod. The registered MethodCallCallback in the Skeleton process
    /// will then be called which calls the actual method and puts the return value in shared memory (if there is one).
    ///
    /// A Skeleton opens a shared memory region for each connected Proxy which contains a method. The provided
    /// ProxyMethodInstanceIdentifier is required to identify which of the connected ProxyMethods the provided callback
    /// corresponds to.
    ///
    /// Each MethodCallHandler stores pointers to the InArg and Return storage in the specific shared memory region
    /// created by the Proxy. For this reason, we need to register a method call handler per ProxyMethod, not per
    /// SkeletonMethod (i.e. because a SkeletonMethod will register different handlers per connected ProxyMethod). Note:
    /// This handler is NOT the user provided handler, but a wrapper around it. We only have one user provided handler
    /// per SkeletonMethod.
    ///
    /// \param asil_level ASIL level of method.
    /// \param proxy_method_instance_identifier to identify which MethodCallHandler to call when CallMethod is called on
    /// the Proxy side
    /// \param method_call_callback callback that will be called when CallMethod is called
    /// \param allowed_proxy_uid The uid of the proxy process which can call the registered handler. Since we register a
    ///        method call handler per ProxyMethod, we can restrict the caller of the handler to the ProxyMethod who
    ///        registered it.
    virtual Result<MethodCallRegistrationGuard> RegisterMethodCallHandler(
        const QualityType asil_level,
        const ProxyMethodInstanceIdentifier proxy_method_instance_identifier,
        MethodCallHandler method_call_callback,
        const uid_t allowed_proxy_uid) = 0;

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
    /// \param asil_level See arguments documentation for ServiceMethodSubscribedHandler.
    /// \param skeleton_instance_identifier identification of the Skeleton corresponding to the Proxy which is calling
    /// this method.
    /// \param proxy_instance_identifier See arguments documentation for ServiceMethodSubscribedHandler.
    /// \param target_node_id Since this function is called by the Proxy process, target_node_id is the PID of the
    ///        Skeleton process which the subscribe call is sent to (i.e. which contains the corresponding Skeleton)
    virtual ResultBlank SubscribeServiceMethod(const QualityType asil_level,
                                               const SkeletonInstanceIdentifier& skeleton_instance_identifier,
                                               const ProxyInstanceIdentifier& proxy_instance_identifier,
                                               const pid_t target_node_id) = 0;

    /// \brief Blocking call which is called on Proxy side to trigger the Skeleton to process a method call. The
    /// callback registered with RegisterOnServiceMethodSubscribed will be called on the Skeleton side and a response
    /// will be returned
    ///
    /// A Skeleton opens a shared memory region for each connected Proxy which contains a method. The provided
    /// ProxyInstanceIdentifier is required to identify which of the connected ProxyMethods has called the method.
    ///
    /// \param asil_level ASIL level of method.
    /// \param proxy_method_instance_identifier identification of the specific ProxyMethod which is calling this method.
    /// \param queue_position The position in the queue of method calls in shared memory relating to the current method
    ///        call. Until asynchronous method calls are supported, this will always be 0.
    /// \param target_node_id Since this function is called by the Proxy process, target_node_id is the PID of the
    ///        Skeleton process which the method call is sent to (i.e. which contains the corresponding Skeleton)
    virtual ResultBlank CallMethod(const QualityType asil_level,
                                   const ProxyMethodInstanceIdentifier& proxy_method_instance_identifier,
                                   const std::size_t queue_position,
                                   const pid_t target_node_id) = 0;

  private:
    /// \brief Unregister handler that was registered with RegisterOnServiceMethodSubscribedHandler
    ///
    /// Removes the handler associated with the provided skeleton_instance_identifier from the internal handler map.
    /// After this call completes, the corresponding handler will no longer be able to be called. However, any currently
    /// executing handlers will continue. This function is private and will only be called by
    /// MethodSubscriptionRegistrationGuardFactory on destruction.
    ///
    /// \pre Shall only be called after RegisterOnServiceMethodSubscribedHandler was successfully called.
    ///
    /// \param asil_level ASIL level of method.
    /// \param skeleton_instance_identifier to identify which registered ServiceMethodSubscribedHandler to unregister
    virtual void UnregisterOnServiceMethodSubscribedHandler(
        const QualityType asil_level,
        SkeletonInstanceIdentifier skeleton_instance_identifier) = 0;

    /// \brief Unregister handler that was registered with RegisterMethodCallHandler
    ///
    /// Removes the handler associated with the provided skeleton_instance_identifier from the internal handler map.
    /// After this call completes, the corresponding handler will no longer be able to be called. However, any currently
    /// executing handlers will continue. This function is private and will only be called by
    /// MethodCallRegistrationGuardFactory on destruction.
    ///
    /// \pre Shall only be called after RegisterMethodCallHandler was successfully called.
    ///
    /// \param asil_level ASIL level of method.
    /// \param proxy_method_instance_identifier to identify which registered MethodCallHandler to unregister
    virtual void UnregisterMethodCallHandler(const QualityType asil_level,
                                             ProxyMethodInstanceIdentifier proxy_method_instance_identifier) = 0;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_IMESSAGEPASSINGSERVICE_H
