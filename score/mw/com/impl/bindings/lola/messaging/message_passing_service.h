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

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance.h"

// TODO: dependency injection?
#ifdef __QNX__
#include "score/message_passing/qnx_dispatch/qnx_dispatch_client_factory.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_server_factory.h"
#else
#include "score/message_passing/unix_domain/unix_domain_client_factory.h"
#include "score/message_passing/unix_domain/unix_domain_server_factory.h"
#endif

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
    using AsilSpecificCfg = MessagePassingServiceInstance::AsilSpecificCfg;

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
    MessagePassingService(const AsilSpecificCfg config_asil_qm,
                          const score::cpp::optional<AsilSpecificCfg> config_asil_b) noexcept;

    MessagePassingService(const MessagePassingService&) = delete;
    MessagePassingService(MessagePassingService&&) = delete;
    MessagePassingService& operator=(const MessagePassingService&) = delete;
    MessagePassingService& operator=(MessagePassingService&&) = delete;

    ~MessagePassingService() noexcept override;

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

    /// \brief Notifies target node about outdated_node_id being an old/outdated node id, not being used anymore.
    /// \details see IMessagePassingService::NotifyOutdatedNodeId
    void NotifyOutdatedNodeId(const QualityType asil_level,
                              const pid_t outdated_node_id,
                              const pid_t target_node_id) noexcept override;

  private:
// TODO: dependency injection?
#ifdef __QNX__
    std::optional<score::message_passing::QnxDispatchServerFactory> server_factory_;
    std::optional<score::message_passing::QnxDispatchClientFactory> client_factory_;
#else
    std::optional<score::message_passing::UnixDomainServerFactory> server_factory_;
    std::optional<score::message_passing::UnixDomainClientFactory> client_factory_;
#endif

    /// \brief thread pool for processing local event update notification.
    /// \detail local update notification leads to a user provided receive handler callout, whose
    ///         runtime is unknown, so we decouple with worker threads.
    score::concurrency::ThreadPool local_event_thread_pool_;
    std::optional<MessagePassingServiceInstance> qm_;
    std::optional<MessagePassingServiceInstance> asil_b_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGSERVICE_H
