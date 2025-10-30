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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGSERVICEINSTANCE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGSERVICEINSTANCE_H

#include "score/message_passing/i_client_factory.h"
#include "score/message_passing/i_server_factory.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_client_cache.h"

// TODO: PMR
#include "score/concurrency/thread_pool.h"

// TODO: PMR
#include <set>
#include <unordered_map>
#include <vector>

#include <iostream>

namespace score::mw::com::impl::lola
{

class MessagePassingServiceInstance
{
  public:
    using ClientQualityType = MessagePassingClientCache::ClientQualityType;

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

    MessagePassingServiceInstance(const ClientQualityType asil_level,
                                  const AsilSpecificCfg config,
                                  score::message_passing::IServerFactory& server_factory,
                                  score::message_passing::IClientFactory& client_factory,
                                  score::concurrency::ThreadPool& local_event_thread_pool) noexcept;

    MessagePassingServiceInstance(const MessagePassingServiceInstance&) = delete;
    MessagePassingServiceInstance(MessagePassingServiceInstance&&) = delete;
    MessagePassingServiceInstance& operator=(const MessagePassingServiceInstance&) = delete;
    MessagePassingServiceInstance& operator=(MessagePassingServiceInstance&&) = delete;

    ~MessagePassingServiceInstance() noexcept = default;

    void NotifyEvent(const ElementFqId event_id) noexcept;

    IMessagePassingService::HandlerRegistrationNoType RegisterEventNotification(
        const ElementFqId event_id,
        std::weak_ptr<ScopedEventReceiveHandler> callback,
        const pid_t target_node_id) noexcept;

    void ReregisterEventNotification(const ElementFqId event_id, const pid_t target_node_id) noexcept;

    void UnregisterEventNotification(const ElementFqId event_id,
                                     const IMessagePassingService::HandlerRegistrationNoType registration_no,
                                     const pid_t target_node_id) noexcept;

    void NotifyOutdatedNodeId(const pid_t outdated_node_id, const pid_t target_node_id) noexcept;

  private:
    enum class MessageType : std::uint8_t
    {
        kRegisterEventNotifier = 1,  //< event notifier registration message sent by proxy_events
        kUnregisterEventNotifier,    //< event notifier un-registration message sent by proxy_events
        kNotifyEvent,                //< event update notification message sent by skeleton_events
        kOutdatedNodeId,  //< outdated node id message (sent from a LoLa process in the role as consumer to the
                          // producer)
    };

    struct RegisteredNotificationHandler
    {
        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types
        // shall be private.". We need these data elements to be organized into a coherent organized data structure.
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

    // TODO: PMR
    using EventUpdateNotifierMapType = std::unordered_map<ElementFqId, std::vector<RegisteredNotificationHandler>>;
    using EventUpdateNodeIdMapType = std::unordered_map<ElementFqId, std::set<pid_t>>;
    using EventUpdateRegistrationCountMapType = std::unordered_map<ElementFqId, NodeCounter>;
    /// \brief tmp buffer for copying ids under lock.
    /// \todo Make its size configurable?
    using NodeIdTmpBufferType = std::array<pid_t, 20>;

    void MessageCallback(const pid_t sender_pid, const score::cpp::span<const std::uint8_t> message) noexcept;
    void HandleNotifyEventMsg(const score::cpp::span<const std::uint8_t> payload, const pid_t sender_node_id) noexcept;
    void HandleRegisterNotificationMsg(const score::cpp::span<const std::uint8_t> payload,
                                       const pid_t sender_node_id) noexcept;
    void HandleUnregisterNotificationMsg(const score::cpp::span<const std::uint8_t> payload,
                                         const pid_t sender_node_id) noexcept;
    void HandleOutdatedNodeIdMsg(const score::cpp::span<const std::uint8_t> payload,
                                 const pid_t sender_node_id) noexcept;

    std::uint32_t NotifyEventLocally(const ElementFqId event_id) noexcept;
    void NotifyEventRemote(const ElementFqId event_id) noexcept;
    void RegisterEventNotificationRemote(const ElementFqId event_id, const pid_t target_node_id) noexcept;
    void UnregisterEventNotificationRemote(const ElementFqId event_id,
                                           const IMessagePassingService::HandlerRegistrationNoType registration_no,
                                           const pid_t target_node_id) noexcept;
    void SendRegisterEventNotificationMessage(const ElementFqId event_id, const pid_t target_node_id) noexcept;

    score::cpp::pmr::unique_ptr<score::message_passing::IServer> server_;

    /// \brief Copies node identifiers (pid) contained within (container) values of a map into a given buffer under
    ///        read-lock
    /// \details
    /// \tparam MapType Type of the Map. It is implicitly expected, that the key of MapType is of type ElementFqId
    /// and
    ///         the mapped_type is a std::set (or at least some forward iterable container type, which supports
    ///         lower_bound()), which contains values, which directly or indirectly contain a node identifier
    ///         (pid_t)
    ///
    /// \param event_id fully qualified event id for lookup in _src_map_
    /// \param src_map map, where key_type = ElementFqId and mapped_type is some forward iterable container
    /// \param src_map_mutex mutex to be used to lock the map during copying.
    /// \param dest_buffer buffer, where to copy the node identifiers
    /// \param start start identifier (pid_t) where to start search with.
    ///
    /// \return pair containing number of node identifiers, which have been copied and a bool, whether further ids
    /// could
    ///         have been copied, if buffer would have been larger.

    template <typename MapType>
    // Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
    // called implicitly".
    // This is a false positive: .at() could throw if the index is outside of the range of the container but it is
    // checked before accessing the dest_buffer that the function will break as soon as the index is equal to the
    // buffersize. So an access out of the range and consequently a call to std::terminate() is not possible.
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    static std::pair<std::uint8_t, bool> CopyNodeIdentifiers(ElementFqId event_id,
                                                             MapType& src_map,
                                                             std::shared_mutex& src_map_mutex,
                                                             NodeIdTmpBufferType& dest_buffer,
                                                             pid_t start) noexcept
    {
        std::uint8_t num_nodeids_copied{0U};
        bool further_ids_avail{false};
        // Suppress "AUTOSAR C++14 M0-1-3" rule findings. This rule states: "There shall be no dead code".
        //  This is a RAII Pattern, which binds the life cycle of a resource that must be acquired before use.
        // coverity[autosar_cpp14_m0_1_3_violation]
        std::shared_lock<std::shared_mutex> read_lock(src_map_mutex);
        if (src_map.empty() == false)
        {
            auto search = src_map.find(event_id);
            if (search != src_map.end())
            {
                // we copy the target_node_identifiers to a tmp-array under lock ...
                for (auto iter = search->second.lower_bound(start); iter != search->second.cend(); iter++)
                {
                    // Suppress "AUTOSAR C++14 M5-0-3" rule findings. This rule states: "A cvalue expression shall
                    // not be implicitly converted to a different underlying type"
                    // That return `pid_t` type as a result. Tolerated implicit conversion as underlying types are
                    // the same
                    // coverity[autosar_cpp14_m5_0_3_violation : FALSE]
                    dest_buffer.at(num_nodeids_copied) = *iter;
                    num_nodeids_copied++;
                    if (num_nodeids_copied == dest_buffer.size())
                    {
                        if (std::distance(iter, search->second.cend()) > 1)
                        {
                            further_ids_avail = true;
                        }
                        break;
                    }
                }
            }
        }
        return {num_nodeids_copied, further_ids_avail};
    }

    std::atomic<IMessagePassingService::HandlerRegistrationNoType> cur_registration_no_;
    MessagePassingClientCache client_cache_;

    // TODO: refactor for PMR/static

    /// \brief map holding per event_id a list of notification/receive handlers registered by local proxy-event
    ///        instances, which need to be called, when the event with given _event_id_ is updated.
    EventUpdateNotifierMapType event_update_handlers_;

    std::shared_mutex event_update_handlers_mutex_;

    /// \brief map holding per event_id a list of remote LoLa nodes, which need to be informed, when the event with
    ///        given _event_id_ is updated.
    /// \note This is the symmetric data structure to event_update_handlers_, in case the proxy-event registering
    ///       a receive handler is located in a different LoLa process.
    EventUpdateNodeIdMapType event_update_interested_nodes_;

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
    EventUpdateRegistrationCountMapType event_update_remote_registrations_;

    std::shared_mutex event_update_remote_registrations_mutex_;

    /// \brief thread pool for processing local event update notification.
    /// \detail local update notification leads to a user provided receive handler callout, whose
    ///         runtime is unknown, so we decouple with worker threads.
    score::concurrency::ThreadPool& thread_pool_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGEPASSINGSERVICEINSTANCE_H
