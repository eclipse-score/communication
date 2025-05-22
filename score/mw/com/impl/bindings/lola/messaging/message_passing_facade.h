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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGFACADE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGFACADE_H

#include "score/concurrency/thread_pool.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_control.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_notify_event_handler.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/message_passing/i_receiver.h"

#include <score/callback.hpp>
#include <score/memory.hpp>
#include <score/optional.hpp>
#include <score/stop_token.hpp>

#include <sys/types.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace score::mw::com::impl::lola
{

namespace test
{
class MessagePassingFacadeAttorney;
}

/// \brief MessagePassingFacade handles message-based communication between LoLa proxy/skeleton instances of different
/// processes.
///
/// \details This message-based communication is a side-channel to the shared-memory based interaction between LoLa
/// proxy/skeleton instances. It is used for exchange of control information/notifications, where the shared-memory
/// channel is used rather for data exchange.
/// MessagePassingFacade relies on message_passing::Receiver/Sender for its communication needs.
/// If it detects, that communication partners are located within the same process, it opts for direct function/method
/// call optimization, instead of using message_passing.
///
class MessagePassingFacade final : public IMessagePassingService
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "MessagePassingFacadeAttorney" class is a helper, which sets the internal state of
    // "MessagePassingFacade" accessing private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend test::MessagePassingFacadeAttorney;

  public:
    /// \brief Aggregation of ASIL level specific/dependent config properties.
    struct AsilSpecificCfg
    {
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". We need these data elements to be organized into a coherent organized data structure.
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::int32_t message_queue_rx_size_;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::vector<uid_t> allowed_user_ids_;
    };

    /// \brief Constructs MessagePassingFacade, which handles the whole inter-process messaging needs for a LoLa enabled
    /// process.
    ///
    /// \details Used by com::impl::Runtime and instantiated only once, since we want to have "singleton" behavior,
    /// without applying singleton pattern.
    ///
    /// \param stop_source Stop source for stopping the NotifyEventHandler
    /// \param notify_event_handler instance to which to dispatch event-notification related calls to.
    /// \param msgpass_ctrl message passing control used for access to node_identifier, etc.
    /// \param config_asil_qm configuration props for ASIL-QM (mandatory) communication path
    /// \param config_asil_b optional (only needed for ASIL-B enabled MessagePassingFacade) configuration props for
    ///                ASIL-B communication path.
    ///                If this optional contains a value, this leads to implicit ASIL-B support of created
    ///                MessagePassingFacade! This optional should only be set, in case the overall
    ///                application/process is implemented according to ASIL_B requirements and there is at least one
    ///                LoLa service deployment (proxy or skeleton) for the process, with asilLevel "ASIL_B".
    MessagePassingFacade(score::cpp::stop_source& stop_source,
                         std::unique_ptr<INotifyEventHandler> notify_event_handler,
                         IMessagePassingControl& msgpass_ctrl,
                         const AsilSpecificCfg config_asil_qm,
                         const score::cpp::optional<AsilSpecificCfg> config_asil_b) noexcept;

    MessagePassingFacade(const MessagePassingFacade&) = delete;
    MessagePassingFacade(MessagePassingFacade&&) = delete;
    MessagePassingFacade& operator=(const MessagePassingFacade&) = delete;
    MessagePassingFacade& operator=(MessagePassingFacade&&) = delete;

    ~MessagePassingFacade() noexcept override;

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
    void ReregisterEventNotification(QualityType asil_level,
                                     ElementFqId event_id,
                                     pid_t target_node_id) noexcept override;

    /// \brief Unregister an event update notification callback, which has been registered with
    ///        RegisterEventNotification()
    /// \details see IMessagePassingService::UnregisterEventNotification
    void UnregisterEventNotification(const QualityType asil_level,
                                     const ElementFqId event_id,
                                     const HandlerRegistrationNoType registration_no,
                                     const pid_t target_node_id) noexcept override;

    /// \brief Notifies target node about outdated_node_id being an old/outdated node id, not being used anymore.
    /// \details see IMessagePassingService::NotifyOutdatedNodeId
    void NotifyOutdatedNodeId(QualityType asil_level, pid_t outdated_node_id, pid_t target_node_id) noexcept override;

  private:
    struct MessageReceiveCtrl
    {
        /// \brief message receiver
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". We need these data elements to be organized into a coherent organized data structure.
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::cpp::pmr::unique_ptr<score::mw::com::message_passing::IReceiver> receiver_;
        /// \brief ... and its thread-pool/execution context.
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::unique_ptr<score::concurrency::ThreadPool> thread_pool_;
    };

    /// \brief initialize receiver_asil_qm_ resp. receiver_asil_b_
    /// \param asil_level
    /// \param min_num_messages
    /// \param allowed_user_ids
    void InitializeMessagePassingReceiver(const QualityType asil_level,
                                          const std::vector<uid_t>& allowed_user_ids,
                                          const std::int32_t min_num_messages) noexcept;

    /// \brief message passing control used to acquire node_identifier and senders.
    IMessagePassingControl& message_passing_ctrl_;

    /// \brief does our instance support ASIL-B?
    bool asil_b_capability_;

    score::cpp::stop_source& stop_source_;

    /// \brief handler for notify-event-update, register-event-notification and unregister-event-notification messages.
    /// \attention Position of this handler member is important as it shall be destroyed AFTER the upcoming receiver
    ///            members to avoid race conditions, as those receivers are using this handler to dispatch messages.
    std::unique_ptr<INotifyEventHandler> notify_event_handler_;

    /// \brief message passing receiver control, where ASIL-QM qualified messages get received
    MessageReceiveCtrl msg_receiver_qm_;

    /// \brief message passing receiver control, where ASIL-B qualified messages get received
    MessageReceiveCtrl msg_receiver_asil_b_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGFACADE_H
