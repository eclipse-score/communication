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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_NOTIFYEVENTHANDLER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_NOTIFYEVENTHANDLER_H

#include "score/concurrency/thread_pool.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_control.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_notify_event_handler.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_common.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_element_fq_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/message_passing/i_receiver.h"
#include "score/mw/com/message_passing/message.h"

#include <score/callback.hpp>
#include <score/stop_token.hpp>

#include <atomic>
#include <memory>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace score::mw::com::impl::lola
{
/// \brief Handles event-notification functionality of MessagePassingFacade.
///
/// \details Functional aspects, which MessagePassingFacade provides are split into different composites/handlers. This
///          class implements the handling of event-notification functionality:
///          It gets (Un)RegisterEventNotification() calls from proxy instances
class NotifyEventHandler final : public INotifyEventHandler
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // This class provides a view to the private members of this class for testing-only. We need this in order to
    // simulate race conditions in a deterministic way in unit tests.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class NotifyEventHandlerAttorney;

  public:
    /// See documentation in NotifyEventHandler
    NotifyEventHandler(IMessagePassingControl& mp_control,
                       const bool asil_b_capability,
                       const score::cpp::stop_token& token) noexcept;

    /// See documentation in NotifyEventHandler
    void RegisterMessageReceivedCallbacks(const QualityType asil_level,
                                          message_passing::IReceiver& receiver) noexcept override;

    /// See documentation in NotifyEventHandler
    void NotifyEvent(const QualityType asil_level, const ElementFqId event_id) noexcept override;

    /// See documentation in NotifyEventHandler
    IMessagePassingService::HandlerRegistrationNoType RegisterEventNotification(
        const QualityType asil_level,
        const ElementFqId event_id,
        std::weak_ptr<ScopedEventReceiveHandler> callback,
        const pid_t target_node_id) noexcept override;

    /// See documentation in NotifyEventHandler
    void ReregisterEventNotification(QualityType asil_level,
                                     ElementFqId event_id,
                                     pid_t target_node_id) noexcept override;

    /// See documentation in NotifyEventHandler
    void UnregisterEventNotification(const QualityType asil_level,
                                     const ElementFqId event_id,
                                     const IMessagePassingService::HandlerRegistrationNoType registration_no,
                                     const pid_t target_node_id) override;

    /// See documentation in NotifyEventHandler
    void NotifyOutdatedNodeId(QualityType asil_level, pid_t outdated_node_id, pid_t target_node_id) noexcept override;

  private:
    /**
     * \brief Searches for handler with given registration number and removes it, if exists
     * @param asil_level ASIL level of the event
     * @param event_id event for which the notification was registered
     * @param registration_no registration number of the handler
     * @return true in case it existed and was removed, else false
     */
    bool RemoveHandlerForNotification(const QualityType asil_level,
                                      const ElementFqId event_id,
                                      const IMessagePassingService::HandlerRegistrationNoType registration_no) noexcept;

    struct RegisteredNotificationHandler
    {
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". We need these data elements to be organized into a coherent organized data structure.
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::weak_ptr<ScopedEventReceiveHandler> handler;
        // coverity[autosar_cpp14_m11_0_1_violation]
        IMessagePassingService::HandlerRegistrationNoType register_no{};
    };

    /// \brief Counter for registered event receive notifications for the given (target) node.
    struct NodeCounter
    {
        // While true that pid_t is not a fixed width integer it is required by the POSIX standard here.
        // coverity[autosar_cpp14_a9_6_1_violation]
        pid_t node_id;
        std::uint16_t counter;
    };

    using EventUpdateNotifierMapType = std::unordered_map<ElementFqId, std::vector<RegisteredNotificationHandler>>;
    using EventUpdateNodeIdMapType = std::unordered_map<ElementFqId, std::set<pid_t>>;
    using EventUpdateRegistrationCountMapType = std::unordered_map<ElementFqId, NodeCounter>;

    struct EventNotificationControlData
    {
        /// \brief map holding per event_id a list of notification/receive handlers registered by local proxy-event
        ///        instances, which need to be called, when the event with given _event_id_ is updated.
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.". We need these data elements to be organized into a coherent organized data structure.
        // coverity[autosar_cpp14_m11_0_1_violation]
        EventUpdateNotifierMapType event_update_handlers_;

        // coverity[autosar_cpp14_m11_0_1_violation]
        std::shared_mutex event_update_handlers_mutex_;

        /// \brief map holding per event_id a list of remote LoLa nodes, which need to be informed, when the event with
        ///        given _event_id_ is updated.
        /// \note This is the symmetric data structure to event_update_handlers_, in case the proxy-event registering
        ///       a receive handler is located in a different LoLa process.
        // coverity[autosar_cpp14_m11_0_1_violation]
        EventUpdateNodeIdMapType event_update_interested_nodes_;

        // coverity[autosar_cpp14_m11_0_1_violation]
        std::shared_mutex event_update_interested_nodes_mutex_;

        /// \brief map holding per event_id a node counter, how many local proxy-event instances have registered a
        ///       receive-handler for this event at the given node. This map only contains events provided by remote
        ///       LoLa processes.
        /// \note we maintain this data structure for performance reasons: We do NOT send for every
        ///       RegisterEventNotification() call for a "remote" event X by a local proxy-event-instance a message
        ///       to the given node redundantly! We rather do a smart (de)multiplexing here by counting the local
        ///       registrars! If the counter goes from 0 to 1, we send a RegisterNotificationMessage to the remote node
        ///       and we send an UnregisterNotificationMessage to the remote node, when the counter gets decremented to
        ///       0 again.
        // coverity[autosar_cpp14_m11_0_1_violation]
        EventUpdateRegistrationCountMapType event_update_remote_registrations_;

        // coverity[autosar_cpp14_m11_0_1_violation]
        std::shared_mutex event_update_remote_registrations_mutex_;

        // coverity[autosar_cpp14_m11_0_1_violation]
        std::atomic<IMessagePassingService::HandlerRegistrationNoType> cur_registration_no_{0U};
    };

    /// \brief Registers event notification at a remote node.
    /// \param asil_level asil level of event, for which notification shall be registered.
    /// \param event_id full qualified event id
    /// \param target_node_id node id of the event provider
    void RegisterEventNotificationRemote(const QualityType asil_level,
                                         const ElementFqId event_id,
                                         const pid_t target_node_id) noexcept;

    /// \brief Unregisters event notification from a remote node.
    /// \param asil_level asil level of event, for which notification shall be unregistered.
    /// \param event_id full qualified event id
    /// \param target_node_id node id of the event provider
    void UnregisterEventNotificationRemote(const QualityType asil_level,
                                           const ElementFqId event_id,
                                           const IMessagePassingService::HandlerRegistrationNoType registration_no,
                                           const pid_t target_node_id);

    /// \brief Notifies event update towards other LoLa processes interested in.
    /// \param asil_level asil level of updated event.
    /// \param event_id full qualified event id
    void NotifyEventRemote(const QualityType asil_level,
                           const ElementFqId event_id,
                           EventNotificationControlData& event_notification_ctrl) noexcept;

    /// \brief Notifies all registered receive handlers (of local proxy events) about an event update.
    /// \param token
    /// \param asil_level
    /// \param event_id
    /// \return count of handlers, that have been called.
    /// \todo: type of return is deduced from max subscriber count, we do support. Right now master seems to be
    ///        EventSlotStatus::SubscriberCount. Include/use here could introduce some ugly dependencies...?
    std::uint32_t NotifyEventLocally(const score::cpp::stop_token& token,
                                     const QualityType asil_level,
                                     const ElementFqId event_id);

    /// \brief internal handler method, when a notify-event message has been received on a receiver.
    /// \details It notifies process local LoLa proxy event instances, which have registered a notification callback for
    ///          the event_id contained in the message.
    ///          It is the analogue to the NotifyEvent() method, which gets called by local skeleton-event instances,
    ///          but gets triggered by skeleton-event-instances from remote LoLa processes.
    /// \param msg_payload payload of notify-event message
    /// \param asil_level asil
    /// level of provider (deduced from receiver instance, where message has been received) \param sender_node_id
    /// node_id of sender (process)
    void HandleNotifyEventMsg(const score::mw::com::message_passing::ShortMessagePayload msg_payload,
                              const QualityType asil_level,
                              const pid_t sender_node_id);

    /// \brief internal handler method, when a register-event-notification message has been received.
    /// \param msg_payload payload of register-event-notification message
    /// \param asil_level asil level of consumer (deduced from receiver instance, where message has been received)
    /// \param sender_node_id node_id of sender (process)
    void HandleRegisterNotificationMsg(const score::mw::com::message_passing::ShortMessagePayload msg_payload,
                                       const QualityType asil_level,
                                       const pid_t sender_node_id);

    /// \brief internal handler method, when an unregister-event-notification message has been received.
    /// \param msg_payload payload of unregister-event-notification message
    /// \param asil_level asil level of consumer (deduced from receiver instance, where message has been received)
    /// \param sender_node_id node_id of sender (process)
    void HandleUnregisterNotificationMsg(const score::mw::com::message_passing::ShortMessagePayload msg_payload,
                                         const QualityType asil_level,
                                         const pid_t sender_node_id);

    /// \brief internal handler method, when an outdated-node-id message has been received.
    /// \param msg_payload payload of outdated-node-id message
    /// \param asil_level asil level of sender (deduced from receiver instance, where message has been received)
    /// \param sender_node_id node_id of sender (process)
    void HandleOutdatedNodeIdMsg(const score::mw::com::message_passing::ShortMessagePayload msg_payload,
                                 const QualityType asil_level,
                                 const pid_t sender_node_id);

    void SendRegisterEventNotificationMessage(const QualityType asil_level,
                                              const ElementFqId event_id,
                                              const pid_t target_node_id) const noexcept;

    EventNotificationControlData control_data_qm_;
    EventNotificationControlData control_data_asil_;

    /// \brief thread pool for processing local event update notification.
    /// \detail local update notification leads to a user provided receive handler callout, whose
    ///         runtime is unknown, so we decouple with worker threads.
    std::unique_ptr<score::concurrency::ThreadPool> thread_pool_;

    /// \brief stop token handed over from parent/facade used to preempt iteration over userland callouts.
    /// \details NotifyEventLocally() is either called from thread_pool owned by this class (see
    //           EventNotificationControlData.thread_pool_) if we have an event-update of a local event or by an
    //           execution context owned by the IReceiver instance, if we have an event-update of a remote event. In the
    //           former case we use the stop_token provided by EventNotificationControlData.thread_pool_. However, in
    //           the latter case we need a different token, where we use this handed over token.
    score::cpp::stop_token token_;

    /// \brief ref to message passing control, which is used to retrieve node_id and get message-passing sender for
    ///        specific target nodes.
    impl::lola::IMessagePassingControl& mp_control_;

    /// \brief do we support ASIL-B comm in addition to QM default?
    const bool asil_b_capability_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_NOTIFYEVENTHANDLER_H
