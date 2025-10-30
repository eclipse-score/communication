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
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance.h"
#include "score/mw/com/impl/bindings/lola/messaging/thread_abstraction.h"

#include "score/message_passing/i_server_connection.h"

#include "score/language/safecpp/safe_math/safe_math.h"

#include "score/os/errno_logging.h"
#include "score/os/unistd.h"

#include "score/mw/log/logging.h"

#include <limits>
#include <variant>

namespace
{

constexpr std::uint32_t kMaxSendSize{9U};

constexpr std::uint8_t kMaxReceiveHandlersPerEvent{5U};

// TODO: make proper serialization
template <typename T>
bool DeserializeFromPayload(const score::cpp::span<const std::uint8_t> payload, T& t) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(sizeof(T) + 1 <= kMaxSendSize);

    if (sizeof(T) != payload.size())
    {
        score::mw::log::LogError("lola") << "Wrong payload size, got " << payload.size() << ", expected " << sizeof(T);
        return false;
    }
    // NOLINTBEGIN(score-banned-function) deserialization of trivially copyable
    // Suppress "AUTOSAR C++14 A12-0-2" The rule states: "Bitwise operations and operations that assume data
    // representation in memory shall not be performed on objects."
    // False-positive: trivially-copyable object
    // Suppress "AUTOSAR C++14 A0-1-2": The return value of memcpy is not needed since it returns a void* to its
    // destination (which is &t)
    // coverity[autosar_cpp14_a12_0_2_violation : FALSE]
    // coverity[autosar_cpp14_a0_1_2_violation]
    std::memcpy(&t, payload.data(), payload.size());
    // NOLINTEND(score-banned-function) deserialization of trivially copyable
    return true;
}

// TODO: make proper serialization
template <typename T>
auto SerializeToMessage(const std::uint8_t message_id, const T& t) noexcept -> std::array<std::uint8_t, sizeof(T) + 1>
{
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(sizeof(T) + 1 <= kMaxSendSize);

    std::array<std::uint8_t, sizeof(T) + 1> out{};
    out[0] = message_id;
    // NOLINTBEGIN(score-banned-function) serialization of trivially copyable
    // Suppress "AUTOSAR C++14 A0-1-2": The return value of memcpy is not needed since it returns a void* to its
    // destination (which is &out)
    // coverity[autosar_cpp14_a0_1_2_violation]
    std::memcpy(&out[1], &t, sizeof(T));
    // NOLINTEND(score-banned-function) deserialization of trivially copyable
    return out;
}

}  // namespace

score::mw::com::impl::lola::MessagePassingServiceInstance::MessagePassingServiceInstance(
    const ClientQualityType asil_level,
    const AsilSpecificCfg /*config*/,
    score::message_passing::IServerFactory& server_factory,
    score::message_passing::IClientFactory& client_factory,
    score::concurrency::ThreadPool& local_event_thread_pool) noexcept
    : cur_registration_no_{0U}, client_cache_{asil_level, client_factory}, thread_pool_{local_event_thread_pool}
{
    // TODO: PMR

    auto node_identifier = score::os::Unistd::instance().getpid();
    auto service_identifier = MessagePassingClientCache::CreateMessagePassingName(asil_level, node_identifier);
    score::message_passing::ServiceProtocolConfig protocol_config{service_identifier, kMaxSendSize, 0U, 0U};
    score::message_passing::IServerFactory::ServerConfig server_config{};
    server_ = server_factory.Create(protocol_config, server_config);

    auto connect_callback = [](score::message_passing::IServerConnection& connection) noexcept -> std::uintptr_t {
        const pid_t client_pid = connection.GetClientIdentity().pid;
        return static_cast<std::uintptr_t>(client_pid);
    };
    auto disconnect_callback = [](score::message_passing::IServerConnection& /*connection*/) noexcept {
        // TODO: outdated node id?
    };
    // Suppress autosar_cpp14_a15_5_3_violation: False Positive
    // Rationale: Passing an argument by reference cannot throw
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    auto received_send_message_callback =
        [this](score::message_passing::IServerConnection& connection,
               const score::cpp::span<const std::uint8_t> message) noexcept -> score::cpp::blank {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(std::holds_alternative<std::uintptr_t>(connection.GetUserData()),
                                                    "Message Passing: UserData does not contain a uintptr_t");
        // Suppress "AUTOSAR C++14 A15-5-3" The rule states: "Implicit call of std::terminate()"
        // Before accessing the variant with std::get it is checked that the variant holds the alternative unitptr_t
        // coverity[autosar_cpp14_a15_5_3_violation]
        auto UserDataUintPtr = std::get<std::uintptr_t>(connection.GetUserData());

        const auto client_pid = score::safe_math::Cast<pid_t>(UserDataUintPtr);
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            client_pid.has_value(), "Message Passing : Message Passing: PID is bigger than pid_t::max()");
        this->MessageCallback(client_pid.value(), message);
        return {};
    };

    auto received_send_message_with_reply_callback =
        // Suppress autosar_cpp14_a15_5_3_violation: False Positive
        // Rationale: Passing an argument by reference cannot throw
        // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
        [](score::message_passing::IServerConnection& connection,
           score::cpp::span<const std::uint8_t> /*message*/) noexcept -> score::cpp::blank {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(std::holds_alternative<std::uintptr_t>(connection.GetUserData()),
                                                    "Message Passing: UserData does not contain a uintptr_t");
        // Suppress "AUTOSAR C++14 A15-5-3" The rule states: "Implicit call of std::terminate()"
        // Before accessing the variant with std::get it is checked that the variant holds the alternative unitptr_t
        // coverity[autosar_cpp14_a15_5_3_violation]
        auto UserDataUintPtr = std::get<std::uintptr_t>(connection.GetUserData());

        const auto client_pid = score::safe_math::Cast<pid_t>(UserDataUintPtr);
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            client_pid.has_value(), "Message Passing : Message Passing: PID is bigger than pid_t::max()");

        score::mw::log::LogError("lola") << "MessagePassingService: Unexpected request from client "
                                         << client_pid.error();
        return {};
    };

    // Suppress "AUTOSAR C++14 A5-1-4" rule finding: "A lambda expression object shall not outlive any of its
    // reference-captured objects.".
    // `server_` is deleted in the destructor of the captured `this`
    auto result = server_->StartListening(connect_callback,
                                          disconnect_callback,
                                          // coverity[autosar_cpp14_a5_1_4_violation]
                                          received_send_message_callback,
                                          received_send_message_with_reply_callback);
    if (!result.has_value())
    {
        score::mw::log::LogFatal("lola") << "MessagePassingService: Failed to start listening on " << service_identifier
                                         << " with following error: " << result.error();
        std::terminate();
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::MessageCallback(
    const pid_t sender_pid,
    const score::cpp::span<const std::uint8_t> message) noexcept
{
    if (message.size() < 1U)
    {
        score::mw::log::LogError("lola") << "MessagePassingService: Empty message received from " << sender_pid;
        return;
    }
    const auto payload = message.subspan(1U);
    switch (message.front())
    {
        case score::cpp::to_underlying(MessageType::kRegisterEventNotifier):
            HandleRegisterNotificationMsg(payload, sender_pid);
            break;
        case score::cpp::to_underlying(MessageType::kUnregisterEventNotifier):
            HandleUnregisterNotificationMsg(payload, sender_pid);
            break;
        case score::cpp::to_underlying(MessageType::kNotifyEvent):
            HandleNotifyEventMsg(payload, sender_pid);
            break;
        case score::cpp::to_underlying(MessageType::kOutdatedNodeId):
            HandleOutdatedNodeIdMsg(payload, sender_pid);
            break;
        default:
            score::mw::log::LogError("lola")
                << "MessagePassingService: Unsupported MessageType received from " << sender_pid;
            break;
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::HandleNotifyEventMsg(
    const score::cpp::span<const std::uint8_t> payload,
    const pid_t sender_node_id) noexcept
{
    // TODO: make proper serialization
    ElementFqId elementFqId{};
    if (!DeserializeFromPayload(payload, elementFqId))
    {
        return;
    }
    if (NotifyEventLocally(elementFqId) == 0U)
    {
        score::mw::log::LogWarn("lola")
            << "MessagePassingService: Received NotifyEventUpdateMessage for event: " << elementFqId.ToString()
            << " from node " << sender_node_id
            << " although we don't have currently any registered handlers. Might be an acceptable "
               "race, if it happens seldom!";
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::HandleRegisterNotificationMsg(
    const score::cpp::span<const std::uint8_t> payload,
    const pid_t sender_node_id) noexcept
{
    // TODO: make proper serialization
    ElementFqId elementFqId{};
    if (!DeserializeFromPayload(payload, elementFqId))
    {
        return;
    }

    bool already_registered{false};

    // TODO: PMR
    std::unique_lock<std::shared_mutex> write_lock(event_update_interested_nodes_mutex_);
    auto search = event_update_interested_nodes_.find(elementFqId);
    if (search != event_update_interested_nodes_.end())
    {
        auto inserted = search->second.insert(sender_node_id);
        already_registered = (inserted.second == false);
    }
    else
    {
        auto emplaced = event_update_interested_nodes_.emplace(elementFqId, std::set<pid_t>{});
        score::cpp::ignore = emplaced.first->second.insert(sender_node_id);
    }
    write_lock.unlock();

    if (already_registered)
    {
        score::mw::log::LogWarn("lola")
            << "MessagePassingService: Received redundant RegisterEventNotificationMessage for event: "
            << elementFqId.ToString() << " from node " << sender_node_id;
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::HandleUnregisterNotificationMsg(
    const score::cpp::span<const std::uint8_t> payload,
    const pid_t sender_node_id) noexcept
{
    // TODO: make proper serialization
    ElementFqId elementFqId{};
    if (!DeserializeFromPayload(payload, elementFqId))
    {
        return;
    }

    bool registration_found{false};

    std::unique_lock<std::shared_mutex> write_lock(event_update_interested_nodes_mutex_);
    auto search = event_update_interested_nodes_.find(elementFqId);
    if (search != event_update_interested_nodes_.end())
    {
        registration_found = search->second.erase(sender_node_id) == 1U;
    }
    write_lock.unlock();

    if (!registration_found)
    {
        score::mw::log::LogWarn("lola")
            << "MessagePassingService: Received UnregisterEventNotificationMessage for event: "
            << elementFqId.ToString() << " from node " << sender_node_id << ", but there was no registration!";
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::HandleOutdatedNodeIdMsg(
    const score::cpp::span<const std::uint8_t> payload,
    const pid_t sender_node_id) noexcept
{
    pid_t pid_to_unregister{};

    if (!DeserializeFromPayload(payload, pid_to_unregister))
    {
        return;
    }

    EventUpdateNodeIdMapType::size_type remove_count{0U};
    std::unique_lock<std::shared_mutex> write_lock(event_update_interested_nodes_mutex_);
    for (auto& element : event_update_interested_nodes_)
    {
        remove_count += element.second.erase(pid_to_unregister);
    }
    write_lock.unlock();

    if (remove_count == 0U)
    {
        score::mw::log::LogInfo("lola") << "MessagePassingService: HandleOutdatedNodeIdMsg for outdated node id:"
                                        << pid_to_unregister << "from node" << sender_node_id
                                        << ". No update notifications for outdated node existed.";
    }

    client_cache_.RemoveMessagePassingClient(pid_to_unregister);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly".
// This is a false positive: .at() could throw if the index is outside of the range of the container but the function
// CopyNodeIdentifiers will only return a value which is in range of the array. Otherwise CopyNodeIdentifiers will break
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void score::mw::com::impl::lola::MessagePassingServiceInstance::NotifyEventRemote(const ElementFqId event_id) noexcept
{
    NodeIdTmpBufferType nodeIdentifiersTmp;
    pid_t start_node_id{0};
    const auto message = SerializeToMessage(score::cpp::to_underlying(MessageType::kNotifyEvent), event_id);
    std::pair<std::uint8_t, bool> num_ids_copied;
    std::uint8_t loop_count{0U};
    do
    {
        if (loop_count == 255U)
        {
            score::mw::log::LogError("lola")
                << "An overflow in counting the node identifiers to notifies event update.";
            break;
        }
        else
        {
            loop_count++;
        }

        num_ids_copied = CopyNodeIdentifiers(event_id,
                                             event_update_interested_nodes_,
                                             event_update_interested_nodes_mutex_,
                                             nodeIdentifiersTmp,
                                             start_node_id);
        // send NotifyEventUpdateMessage to each node_id in nodeIdentifiersTmp
        for (std::uint8_t i = 0U; i < num_ids_copied.first; i++)
        {
            // Suppress "AUTOSAR C++14 M5-0-3" rule findings. This rule states: "A cvalue expression shall
            // not be implicitly converted to a different underlying type"
            // False positive, nodeIdentifiersTmp is an array of `pid_t`.
            // coverity[autosar_cpp14_m5_0_3_violation]
            const pid_t node_identifier = nodeIdentifiersTmp.at(i);
            // Suppress "AUTOSAR C++14 A18-5-8" rule finding. This rule states: "Objects that do not outlive a function
            // shall have automatic storage duration". The object is a shared_ptr which is allocated in the heap.
            // coverity[autosar_cpp14_a18_5_8_violation]
            auto sender = client_cache_.GetMessagePassingClient(node_identifier);
            const auto result = sender->Send(message);
            if (!result.has_value())
            {
                score::mw::log::LogError("lola")
                    << "MessagePassingService: Sending NotifyEventUpdateMessage to node_id " << node_identifier
                    << " failed with error: " << result.error();
            }
        }
        if (num_ids_copied.second == true)
        {
            // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to
            // data loss". std::set is a sorted set of unique objects so the biggest element is the last one, and the
            // previous condition will be true only if the distance between current node id and last node id in the map
            // is more than one. So, no way for overflow.
            // coverity[autosar_cpp14_a4_7_1_violation]
            start_node_id = nodeIdentifiersTmp.back() + 1;
        }
    } while (num_ids_copied.second == true);

    if (loop_count > 1U)
    {
        score::mw::log::LogWarn("lola")
            << "MessagePassingService: NotifyEventRemote did need more than one copy loop for "
               "node_identifiers. Think about extending capacity of NodeIdTmpBufferType!";
    }
}

std::uint32_t score::mw::com::impl::lola::MessagePassingServiceInstance::NotifyEventLocally(
    const ElementFqId event_id) noexcept
{
    std::uint32_t handlers_called{0U};

    std::shared_lock<std::shared_mutex> read_lock(event_update_handlers_mutex_);
    auto search = event_update_handlers_.find(event_id);
    if ((search == event_update_handlers_.end()) || (search->second.empty()))
    {
        return handlers_called;
    }

    // copy handlers to tmp-storage
    // tmp-storage for all handlers (weak_ptrs), which will get filled under read-lock
    std::array<std::weak_ptr<ScopedEventReceiveHandler>, kMaxReceiveHandlersPerEvent> handler_weak_ptrs{
        {{}, {}, {}, {}, {}}};
    std::uint8_t number_weak_ptrs_copied{0U};
    auto& handlers_for_event = search->second;
    auto handler_it = handlers_for_event.cbegin();
    while ((handler_it != handlers_for_event.cend()) && (number_weak_ptrs_copied <= kMaxReceiveHandlersPerEvent))
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index): number_weak_ptrs_copied is assured to be
        // always <= kMaxReceiveHandlersPerEvent, which is the array size. So no out-of-bounds access possible.
        handler_weak_ptrs[number_weak_ptrs_copied] = handler_it->handler;
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index): see above
        number_weak_ptrs_copied++;
        handler_it++;
    }
    const auto all_handlers_copied = handler_it == handlers_for_event.cend();
    read_lock.unlock();

    if (!all_handlers_copied)
    {
        score::mw::log::LogError("lola")
            << "MessagePassingServiceInstance: NotifyEventLocally failed to call ALL registered event receive handlers "
               "for event_id"
            << event_id.ToString() << ", because number is exceeding " << kMaxReceiveHandlersPerEvent;
    }

    // Call the handlers outside the read-lock
    for (std::uint8_t i = 0U; i < number_weak_ptrs_copied; i++)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): "i" is assured to be within array bounds.
        if (auto current_handler = handler_weak_ptrs[i].lock())
        {
            auto& scoped_func = (*current_handler);
            // return value tells us, whether the scope has already expired (thus handler not called) or not. We don't
            // care about this!
            score::cpp::ignore = scoped_func();
            // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall
            // not lead to data loss.". handlers_called can't overflow here as it starts with 0 and max loop-count is
            // 255
            // coverity[autosar_cpp14_a4_7_1_violation]
            handlers_called++;
        }
    }
    return handlers_called;
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::NotifyEvent(const ElementFqId event_id) noexcept
{
    // first we forward notification of event update to other LoLa processes, which are interested in this notification.
    // we do this first as message-sending is done synchronous/within the calling thread as it has "short"/deterministic
    // runtime.
    NotifyEventRemote(event_id);

    // Notification of local proxy_events/user receive handlers is decoupled via worker-threads, as user level receive
    // handlers may have an unknown/non-deterministic long runtime.
    std::shared_lock<std::shared_mutex> read_lock(event_update_handlers_mutex_);
    auto search = event_update_handlers_.find(event_id);
    if ((search != event_update_handlers_.end()) && (search->second.empty() == false))
    {
        read_lock.unlock();
        thread_pool_.Post(
            [this](const score::cpp::stop_token& /*token*/, const ElementFqId element_id) noexcept {
                // ignoring the result (number of actually notified local proxy-events),
                // as we don't have any expectation, how many are there.
                score::cpp::ignore = this->NotifyEventLocally(element_id);
            },
            event_id);
    }
}

score::mw::com::impl::lola::IMessagePassingService::HandlerRegistrationNoType
score::mw::com::impl::lola::MessagePassingServiceInstance::RegisterEventNotification(
    const ElementFqId event_id,
    std::weak_ptr<ScopedEventReceiveHandler> callback,
    const pid_t target_node_id) noexcept
{
    std::unique_lock<std::shared_mutex> write_lock(event_update_handlers_mutex_);

    // The rule against increment and decriment operators mixed with others on the same line sems to be more applicable
    // to arithmetic operators. Here the only other operator is the member access operator (i.e. the dot operator) and
    // it in no way confusing to combine it with the increment operator.
    // This rule is also absent from the new MISRA 2023 standart, thus it is in general reasonable to deviate from it.
    // coverity[autosar_cpp14_m5_2_10_violation]
    const IMessagePassingService::HandlerRegistrationNoType registration_no = cur_registration_no_++;
    RegisteredNotificationHandler newHandler{std::move(callback), registration_no};
    auto search = event_update_handlers_.find(event_id);
    if (search != event_update_handlers_.end())
    {
        search->second.push_back(std::move(newHandler));
    }
    else
    {
        auto result = event_update_handlers_.emplace(event_id, std::vector<RegisteredNotificationHandler>{});
        result.first->second.push_back(std::move(newHandler));
    }
    write_lock.unlock();

    if (target_node_id != score::os::Unistd::instance().getpid())
    {
        RegisterEventNotificationRemote(event_id, target_node_id);
    }

    return registration_no;
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::ReregisterEventNotification(
    const score::mw::com::impl::lola::ElementFqId event_id,
    const pid_t target_node_id) noexcept
{
    std::shared_lock<std::shared_mutex> read_lock(event_update_handlers_mutex_);
    auto search = event_update_handlers_.find(event_id);
    if (search == event_update_handlers_.end())
    {
        read_lock.unlock();
        // no registered handler for given event_id -> log as error
        score::mw::log::LogError("lola") << "MessagePassingService: ReregisterEventNotification called for event_id"
                                         << event_id.ToString() << ", which had not yet been registered!";
        return;
    }
    read_lock.unlock();

    // we only do re-register activity, if it is a remote node
    const auto is_target_remote_node = (target_node_id != score::os::Unistd::instance().getpid());
    if (is_target_remote_node)
    {
        std::unique_lock<std::shared_mutex> remote_reg_write_lock(event_update_remote_registrations_mutex_);
        auto registration_count = event_update_remote_registrations_.find(event_id);
        if (registration_count == event_update_remote_registrations_.end())
        {
            remote_reg_write_lock.unlock();
            score::mw::log::LogError("lola") << "MessagePassingService: ReregisterEventNotification for a remote event "
                                             << event_id.ToString() << " without current remote registration!";
            return;
        }
        if (registration_count->second.node_id == target_node_id)
        {
            // we aren't the 1st proxy to re-register. Another proxy already re-registered the event with the new remote
            // pid.

            // The rule against increment and decriment operators mixed with others on the same line sems to be more
            // applicable to arithmetic operators. Here the only other operator is the member access operator (i.e. the
            // dot operator) and it in no way confusing to combine it with the increment operator. This rule is also
            // absent from the new MISRA 2023 standart, thus it is in general reasonable to deviate from it.
            // coverity[autosar_cpp14_m5_2_10_violation]
            registration_count->second.counter++;
            remote_reg_write_lock.unlock();
        }
        else
        {
            // we are the 1st proxy to re-register.
            registration_count->second.node_id = target_node_id;
            registration_count->second.counter = 1U;
            remote_reg_write_lock.unlock();
            SendRegisterEventNotificationMessage(event_id, target_node_id);
        }
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::UnregisterEventNotification(
    const ElementFqId event_id,
    const IMessagePassingService::HandlerRegistrationNoType registration_no,
    const pid_t target_node_id) noexcept
{
    bool found{false};

    std::unique_lock<std::shared_mutex> write_lock(event_update_handlers_mutex_);
    auto search = event_update_handlers_.find(event_id);
    if (search != event_update_handlers_.end())
    {
        // we can do a binary search here, as the registered handlers in this vector are inherently sorted as we emplace
        // always back with monotonically increasing registration number.
        auto result =
            std::lower_bound(search->second.begin(),
                             search->second.end(),
                             registration_no,
                             [](const RegisteredNotificationHandler& regHandler,
                                const IMessagePassingService::HandlerRegistrationNoType value) noexcept -> bool {
                                 return regHandler.register_no < value;
                             });
        if ((result != search->second.end()) && (result->register_no == registration_no))
        {
            score::cpp::ignore = search->second.erase(result);
            found = true;
        }
    }
    write_lock.unlock();
    if (!found)
    {
        score::mw::log::LogWarn("lola")
            << "MessagePassingService: Couldn't find handler for UnregisterEventNotification call with register_no "
            << registration_no;
        // since we didn't find a handler with the given registration_no, we directly return as we have to assume,
        // that this simply is a bogus/wrong unregister call from application level.
        return;
    }

    if (target_node_id != score::os::Unistd::instance().getpid())
    {
        UnregisterEventNotificationRemote(event_id, registration_no, target_node_id);
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::NotifyOutdatedNodeId(
    const pid_t outdated_node_id,
    const pid_t target_node_id) noexcept
{
    const auto message = SerializeToMessage(score::cpp::to_underlying(MessageType::kOutdatedNodeId), outdated_node_id);

    // Suppress "AUTOSAR C++14 A18-5-8" rule finding. This rule states: "Objects that do not outlive a function shall
    // have automatic storage duration".
    // The object is a shared_ptr which is allocated in the heap.
    // coverity[autosar_cpp14_a18_5_8_violation]
    auto sender = client_cache_.GetMessagePassingClient(target_node_id);
    const auto result = sender->Send(message);
    if (!result.has_value())
    {
        score::mw::log::LogError("lola") << "MessagePassingService: Sending OutdatedNodeIdMessage to node_id "
                                         << target_node_id << " failed with error: " << result.error();
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::RegisterEventNotificationRemote(
    const ElementFqId event_id,
    const pid_t target_node_id) noexcept
{
    std::unique_lock<std::shared_mutex> remote_reg_write_lock(event_update_remote_registrations_mutex_);
    const auto registration_count_inserted =
        event_update_remote_registrations_.emplace(std::piecewise_construct,
                                                   std::forward_as_tuple(event_id),
                                                   std::forward_as_tuple(NodeCounter{target_node_id, 1U}));
    if (registration_count_inserted.second == false)
    {
        if (registration_count_inserted.first->second.node_id != target_node_id)
        {
            score::mw::log::LogError("lola")
                << "MessagePassingService: RegisterEventNotificationRemote called for event" << event_id.ToString()
                << "and node_id" << target_node_id << "although event is "
                << " currently located at node" << registration_count_inserted.first->second.node_id;
            registration_count_inserted.first->second.node_id = target_node_id;
            registration_count_inserted.first->second.counter = 1U;
        }
        else
        {
            // The rule against increment and decriment operators mixed with others on the same line sems to be more
            // applicable to arithmetic operators. Here the only other operator is the member access operator (i.e. the
            // dot operator) and it in no way confusing to combine it with the increment operator. This rule is also
            // absent from the new MISRA 2023 standart, thus it is in general reasonable to deviate from it.
            // coverity[autosar_cpp14_m5_2_10_violation]
            registration_count_inserted.first->second.counter++;
        }
    }
    const std::uint16_t reg_counter{registration_count_inserted.first->second.counter};
    remote_reg_write_lock.unlock();
    // only if the counter of registrations switched to 1, we send a message to the remote node.
    if (reg_counter == 1U)
    {
        SendRegisterEventNotificationMessage(event_id, target_node_id);
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::UnregisterEventNotificationRemote(
    const ElementFqId event_id,
    const IMessagePassingService::HandlerRegistrationNoType registration_no,
    const pid_t target_node_id) noexcept
{
    bool send_message{false};
    std::unique_lock<std::shared_mutex> remote_reg_write_lock(event_update_remote_registrations_mutex_);
    auto registration_count = event_update_remote_registrations_.find(event_id);
    if (registration_count == event_update_remote_registrations_.end())
    {
        remote_reg_write_lock.unlock();
        score::mw::log::LogError("lola")
            << "MessagePassingService: UnregisterEventNotification called with register_no " << registration_no
            << " for a remote event " << event_id.ToString() << " without current remote registration!";
        return;
    }
    else
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            registration_count->second.counter > 0U,
            "MessagePassingService: UnregisterEventNotification trying to decrement counter, which is already 0!");
        if (registration_count->second.node_id != target_node_id)
        {
            remote_reg_write_lock.unlock();
            score::mw::log::LogError("lola")
                << "MessagePassingService: UnregisterEventNotification called with register_no " << registration_no
                << " for a remote event " << event_id.ToString() << " for target_node_id" << target_node_id
                << ", which is not the node_id, by which this event is currently provided:"
                << registration_count->second.node_id;
            return;
        }

        // The rule against increment and decriment operators mixed with others on the same line sems to be more
        // applicable to arithmetic operators. Here the only other operator is the member access operator (i.e. the dot
        // operator) and it in no way confusing to combine it with the decriment operator. This rule is also absent from
        // the new MISRA 2023 standart, thus it is in general reasonable to deviate from it.
        // coverity[autosar_cpp14_m5_2_10_violation]
        registration_count->second.counter--;
        // only if the counter of registrations switched back to 0, we send a message to the remote node.
        if (registration_count->second.counter == 0U)
        {
            send_message = true;
            score::cpp::ignore = event_update_remote_registrations_.erase(registration_count);
        }
        remote_reg_write_lock.unlock();
    }

    if (send_message)
    {
        const auto message =
            SerializeToMessage(score::cpp::to_underlying(MessageType::kUnregisterEventNotifier), event_id);
        // Suppress "AUTOSAR C++14 A18-5-8" rule finding. This rule states: "Objects that do not outlive a function
        // shall have automatic storage duration". The object is a shared_ptr which is allocated in the heap.
        // coverity[autosar_cpp14_a18_5_8_violation]
        auto sender = client_cache_.GetMessagePassingClient(target_node_id);
        const auto result = sender->Send(message);
        if (!result.has_value())
        {
            score::mw::log::LogError("lola")
                << "MessagePassingService: Sending UnregisterEventNotificationMessage to node_id " << target_node_id
                << " failed with error: " << result.error();
        }
    }
}

void score::mw::com::impl::lola::MessagePassingServiceInstance::SendRegisterEventNotificationMessage(
    const ElementFqId event_id,
    const pid_t target_node_id) noexcept
{
    const auto message = SerializeToMessage(score::cpp::to_underlying(MessageType::kRegisterEventNotifier), event_id);
    // Suppress "AUTOSAR C++14 A18-5-8" rule finding. This rule states: "Objects that do not outlive a function shall
    // have automatic storage duration".
    // The object is a shared_ptr which is allocated in the heap.
    // coverity[autosar_cpp14_a18_5_8_violation]
    auto sender = client_cache_.GetMessagePassingClient(target_node_id);
    const auto result = sender->Send(message);
    if (!result.has_value())
    {
        score::mw::log::LogError("lola")
            << "MessagePassingService: Sending RegisterEventNotificationMessage to node_id " << target_node_id
            << " failed with error: " << result.error();
    }
}
