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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGSERVICE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGSERVICE_H

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"

#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service_instance.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service_instance_factory.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/configuration/global_configuration.h"

#include "score/concurrency/thread_pool.h"

#ifdef __QNX__
#include "score/message_passing/qnx_dispatch/qnx_dispatch_client_factory.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_server_factory.h"
#else
#include "score/message_passing/unix_domain/unix_domain_client_factory.h"
#include "score/message_passing/unix_domain/unix_domain_server_factory.h"
#endif

#include "score/concurrency/thread_pool.h"

#include <memory>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief MessagePassingService handles message-based communication between LoLa proxy/skeleton instances of different
/// processes.
///
/// \details This message-based communication is a side-channel to the shared-memory based interaction between LoLa
/// proxy/skeleton instances. It is used for exchange of control information/notifications, where the shared-memory
/// channel is used rather for data exchange.
/// MessagePassingService relies on message_passing::Client/Server for its communication needs.
/// If it detects, that communication partners are located within the same process, it opts for direct function/method
/// call optimization, instead of using message_passing.
///
class MessagePassingService final : public IMessagePassingService
{
  public:
    /// \brief Constructs MessagePassingService, which handles the whole inter-process messaging needs for a LoLa
    /// enabled process.
    ///
    /// \details Used by com::impl::Runtime and instantiated only once, since we want to have "singleton" behavior,
    /// without applying singleton pattern.
    ///
    /// \param config_asil_qm configuration props for ASIL-QM (mandatory) communication path
    /// \param config_asil_b optional (only needed for ASIL-B enabled MessagePassingService) configuration props for
    ///                ASIL-B communication path.
    ///                If this optional contains a value, this leads to implicit ASIL-B support of created
    ///                MessagePassingService! This optional should only be set, in case the overall
    ///                application/process is implemented according to ASIL_B requirements and there is at least one
    ///                LoLa service deployment (proxy or skeleton) for the process, with asilLevel "ASIL_B".
    /// \param factory optional factory used to create MessagePassingServiceInstances
    MessagePassingService(const AsilSpecificCfg& config_asil_qm,
                          const std::optional<AsilSpecificCfg>& config_asil_b,
                          const std::unique_ptr<IMessagePassingServiceInstanceFactory>& factory) noexcept;

    MessagePassingService(const MessagePassingService&) = delete;
    MessagePassingService(MessagePassingService&&) = delete;
    MessagePassingService& operator=(const MessagePassingService&) = delete;
    MessagePassingService& operator=(MessagePassingService&&) = delete;

    ~MessagePassingService() noexcept override = default;

    /// \brief Notification, that the given _event_id_ with _asil_level_ has been updated.
    /// \details see IMessagePassingService::NotifyEvent
    void NotifyEvent(const QualityType asil_level, const ElementFqId event_id) noexcept override;

    /// \brief Registers a callback for event update notifications for event _event_id_
    /// \details see IMessagePassingService::RegisterEventNotification
    HandlerRegistrationNoType RegisterEventNotification(const QualityType asil_level,
                                                        const ElementFqId event_id,
                                                        std::weak_ptr<ScopedEventReceiveHandler> callback,
                                                        const pid_t target_node_id) noexcept override;

    /// \brief Re-registers an event update notifications for event _event_id_ in case target_node_id is a remote pid.
    /// \details see IMessagePassingService::ReregisterEventNotification
    void ReregisterEventNotification(const QualityType asil_level,
                                     const ElementFqId event_id,
                                     const pid_t target_node_id) noexcept override;

    /// \brief Unregister an event update notification callback, which has been registered with
    ///        RegisterEventNotification()
    /// \details see IMessagePassingService::UnregisterEventNotification
    void UnregisterEventNotification(const QualityType asil_level,
                                     const ElementFqId event_id,
                                     const HandlerRegistrationNoType registration_no,
                                     const pid_t target_node_id) noexcept override;

    /// \brief Register a handler on Skeleton side which will be called when SubscribeServiceMethod is called by a
    /// Proxy.
    /// \details see IMessagePassingService::RegisterOnServiceMethodSubscribedHandler
    ResultBlank RegisterOnServiceMethodSubscribedHandler(SkeletonInstanceIdentifier skeleton_instance_identifier,
                                                         ServiceMethodSubscribedHandler subscribed_callback) override;

    /// \brief Register a handler on Skeleton side which will be called when CallMethod is called by a Proxy.
    /// \details see IMessagePassingService::RegisterMethodCallHandler
    ResultBlank RegisterMethodCallHandler(ProxyInstanceIdentifier proxy_instance_identifier,
                                          MethodCallHandler method_call_callback) override;

    /// \brief Notifies target node about outdated_node_id being an old/outdated node id, not being used anymore.
    /// \details see IMessagePassingService::NotifyOutdatedNodeId
    void NotifyOutdatedNodeId(const QualityType asil_level,
                              const pid_t outdated_node_id,
                              const pid_t target_node_id) noexcept override;

    /// \brief Registers a callback for event notification existence changes.
    /// \details See IMessagePassingService::RegisterEventNotificationExistenceChangedCallback for detailed
    /// documentation.
    void RegisterEventNotificationExistenceChangedCallback(const QualityType asil_level,
                                                           const ElementFqId event_id,
                                                           HandlerStatusChangeCallback callback) noexcept override;

    /// \brief Unregisters the callback for event notification existence changes.
    /// \details See IMessagePassingService::UnregisterEventNotificationExistenceChangedCallback for detailed
    /// documentation.
    void UnregisterEventNotificationExistenceChangedCallback(const QualityType asil_level,
                                                             const ElementFqId event_id) noexcept override;

    /// \brief Blocking call which is called on Proxy side to notify the Skeleton that a Proxy has setup the
    /// method shared memory region and wants to subscribe. The callback registered with RegisterMethodCall will be
    /// called on the Skeleton side and a response will be returned.
    /// \details see IMessagePassingService::SubscribeServiceMethod
    ResultBlank SubscribeServiceMethod(const SkeletonInstanceIdentifier& skeleton_instance_identifier) override;

    /// \brief Blocking call which is called on Proxy side to trigger the Skeleton to process a method call. The
    /// callback registered with RegisterOnServiceMethodSubscribed will be called on the Skeleton side and a response
    /// will be returned.
    /// \details see IMessagePassingService::CallMethod
    ResultBlank CallMethod(const ProxyInstanceIdentifier& proxy_instance_identifier,
                           std::size_t queue_position) override;

  private:
// Suppress "AUTOSAR C++14 A16-0-1" rule findings.
// This is the standard way to determine if it runs on QNX or Unix
// coverity[autosar_cpp14_a16_0_1_violation]
#ifdef __QNX__
    score::message_passing::QnxDispatchClientFactory client_factory_;
    // coverity[autosar_cpp14_a16_0_1_violation]
#else
    score::message_passing::UnixDomainClientFactory client_factory_;
    // coverity[autosar_cpp14_a16_0_1_violation]
#endif

    /// \brief thread pool for processing local event update notification.
    /// \detail local update notification leads to a user provided receive handler callout, whose
    ///         runtime is unknown, so we decouple with worker threads.
    score::concurrency::ThreadPool local_event_thread_pool_;
    std::unique_ptr<IMessagePassingServiceInstance> qm_;
    std::unique_ptr<IMessagePassingServiceInstance> asil_b_;

    IMessagePassingServiceInstance& GetMessagePassingServiceInstance(const QualityType asil_level) const;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGSERVICE_H
