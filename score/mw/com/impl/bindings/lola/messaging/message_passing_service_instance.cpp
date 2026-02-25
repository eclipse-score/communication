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

#include "score/mw/com/impl/bindings/lola/messaging/client_quality_type.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/methods/method_error.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/error_serializer.h"

#include "score/language/safecpp/safe_math/safe_math.h"
#include "score/language/safecpp/scoped_function/move_only_scoped_function.h"
#include "score/message_passing/i_client_factory.h"
#include "score/message_passing/i_server_connection.h"
#include "score/message_passing/i_server_factory.h"
#include "score/message_passing/service_protocol_config.h"
#include "score/os/errno_logging.h"
#include "score/os/unistd.h"
#include "score/result/error.h"
#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/span.hpp>

#include <sys/types.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

struct SubscribeServiceMethodUnserializedPayload
{
    SkeletonInstanceIdentifier skeleton_instance_identifier;
    ProxyInstanceIdentifier proxy_instance_identifier;
};

struct MethodCallUnserializedPayload
{
    ProxyMethodInstanceIdentifier proxy_method_instance_identifier;
    std::size_t queue_position;
};

using MethodUnserializedReply = score::ResultBlank;
using MethodReplyPayload = ErrorSerializer<MethodErrc>::SerializedErrorType;

constexpr std::uint32_t kMaxSendSize{32U};
constexpr std::uint32_t kMaxReplySize{32U};

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
    // coverity[autosar_cpp14_a12_0_2_violation : FALSE]
    score::cpp::ignore = std::memcpy(&t, payload.data(), payload.size());
    // NOLINTEND(score-banned-function) deserialization of trivially copyable
    return true;
}

/// \brief Function which deserializes a reply payload which is received when calling SendWaitReply in CallMethod or
/// SubscribeServiceMethod.
/// \return The outer result returns an error if SendWaitReply itself returned an error. The inner ResultBlank contains
/// the result encoded in the message itself.
score::Result<MethodUnserializedReply> DeserializeFromMethodReplyPayload(
    const score::cpp::span<const std::uint8_t> payload) noexcept
{
    static_assert(std::is_trivially_copyable_v<MethodReplyPayload>);
    static_assert(sizeof(MethodReplyPayload) <= kMaxSendSize);

    if (sizeof(MethodReplyPayload) != payload.size())
    {
        score::mw::log::LogError("lola") << "Wrong payload size, got " << payload.size() << ", expected "
                                       << sizeof(MethodReplyPayload);
        return MakeUnexpected(MethodErrc::kUnexpectedMessageSize);
    }

    MethodReplyPayload method_payload{};
    // NOLINTBEGIN(score-banned-function) We are copying a buffer into a serialized integer type (which is trivially
    // copyable) into an integer. We check via an assert that the output integer fits the serialized integer. The
    // destination address is stack variable so cannot overlap with the provided payload buffer.
    score::cpp::ignore = std::memcpy(&method_payload, payload.data(), payload.size());
    // NOLINTEND(score-banned-function) deserialization of trivially copyable

    const auto reported_result = ErrorSerializer<MethodErrc>::Deserialize(method_payload);
    return MethodUnserializedReply{reported_result};
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
    score::cpp::ignore = std::memcpy(&out[1], &t, sizeof(T));
    // NOLINTEND(score-banned-function) deserialization of trivially copyable
    return out;
}

auto SerializeToMethodReplyMessage(score::ResultBlank reply_result) noexcept
    -> std::array<std::uint8_t, sizeof(MethodReplyPayload)>
{
    static_assert(std::is_trivially_copyable_v<MethodReplyPayload>);
    static_assert(sizeof(MethodReplyPayload) <= kMaxReplySize);

    const MethodReplyPayload serialized_com_errc =
        reply_result.has_value() ? ErrorSerializer<MethodErrc>::SerializeSuccess()
                                 : ErrorSerializer<MethodErrc>::SerializeError(static_cast<MethodErrc>(
                                       *reply_result.error()));  /// TODO: Use proper error casting

    std::array<std::uint8_t, sizeof(MethodReplyPayload)> out{};

    // NOLINTBEGIN(score-banned-function) We are copying an integer type which is trivially copyable and the destination
    // array is clearly sized to fit the integer which is copied in. The source and destination addresses are both stack
    // variables so cannot be overlapping.
    score::cpp::ignore = std::memcpy(out.data(), &serialized_com_errc, sizeof(MethodReplyPayload));
    // NOLINTEND(score-banned-function) deserialization of trivially copyable
    return out;
}

bool IsMethodErrorRecoverable(const score::result::Error error)
{
    const auto error_code = *error;

    // We static cast each individual Method error code to the base integer type to avoid coverity warnings about static
    // casting the generic error code (from *error) to a specific error code (i.e. MethodErrc)
    if ((error_code == static_cast<result::ErrorCode>(MethodErrc::kSkeletonAlreadyDestroyed)) ||
        (error_code == static_cast<result::ErrorCode>(MethodErrc::kUnknownProxy)) ||
        (error_code == static_cast<result::ErrorCode>(MethodErrc::kNotSubscribed)) ||
        (error_code == static_cast<result::ErrorCode>(MethodErrc::kNotOffered)))
    {
        return true;
    }
    else if ((error_code == static_cast<result::ErrorCode>(MethodErrc::kUnexpectedMessage)) ||
             (error_code == static_cast<result::ErrorCode>(MethodErrc::kUnexpectedMessageSize)) ||
             (error_code == static_cast<result::ErrorCode>(MethodErrc::kMessagePassingError)))
    {
        return false;
    }
    else
    {
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(false, "Provided error is not part of subset relating to methods.");
    }
}

}  // namespace

MessagePassingServiceInstance::MessagePassingServiceInstance(const ClientQualityType asil_level,
                                                             AsilSpecificCfg /*config*/,
                                                             score::message_passing::IServerFactory& server_factory,
                                                             score::message_passing::IClientFactory& client_factory,
                                                             score::concurrency::Executor& local_event_executor) noexcept
    : IMessagePassingServiceInstance(),
      cur_registration_no_{0U},
      asil_level_{asil_level},
      client_cache_{asil_level, client_factory},
      event_update_handlers_{},
      event_update_handlers_mutex_{},
      handler_status_change_callbacks_{},
      handler_status_change_callbacks_mutex_{},
      event_update_interested_nodes_{},
      event_update_interested_nodes_mutex_{},
      event_update_remote_registrations_{},
      event_update_remote_registrations_mutex_{},
      subscribe_service_method_handlers_{},
      subscribe_service_method_handlers_mutex_{},
      call_method_handlers_{},
      call_method_handlers_mutex_{},
      executor_{local_event_executor},
      message_callback_scope_{},
      self_pid_{os::Unistd::instance().getpid()},
      self_uid_{os::Unistd::instance().getuid()}
{
    // TODO: PMR

    auto service_identifier = MessagePassingClientCache::CreateMessagePassingName(asil_level, self_pid_);
    score::message_passing::ServiceProtocolConfig protocol_config{service_identifier, kMaxSendSize, kMaxReplySize, 0U};
    score::message_passing::IServerFactory::ServerConfig server_config{};
    server_ = server_factory.Create(protocol_config, server_config);

    auto connect_callback = [](score::message_passing::IServerConnection& connection) noexcept -> std::uintptr_t {
        const pid_t client_pid = connection.GetClientIdentity().pid;
        return static_cast<std::uintptr_t>(client_pid);
    };
    auto disconnect_callback = [](score::message_passing::IServerConnection& /*connection*/) noexcept {
        // TODO: outdated node id?
        // TODO: update related unit test as well
    };

    auto message_callback_scoped_function =
        std::make_shared<score::safecpp::MoveOnlyScopedFunction<void(pid_t, score::cpp::span<const std::uint8_t>)>>(
            message_callback_scope_, [this](pid_t sender_pid, score::cpp::span<const std::uint8_t> message) noexcept -> void {
                this->MessageCallback(sender_pid, message);
            });
    // Suppress autosar_cpp14_a15_5_3_violation: False Positive
    // Rationale: Passing an argument by reference cannot throw
    // coverity[autosar_cpp14_a15_5_3_violation : FALSE]
    auto received_send_message_callback = [scoped_function = message_callback_scoped_function](
                                              score::message_passing::IServerConnection& connection,
                                              const score::cpp::span<const std::uint8_t> message) noexcept -> score::cpp::blank {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(std::holds_alternative<std::uintptr_t>(connection.GetUserData()),
                               "Message Passing: UserData does not contain a uintptr_t");
        // Suppress "AUTOSAR C++14 A15-5-3" The rule states: "Implicit call of std::terminate()"
        // Before accessing the variant with std::get it is checked that the variant holds the alternative unitptr_t
        // coverity[autosar_cpp14_a15_5_3_violation]
        auto UserDataUintPtr = std::get<std::uintptr_t>(connection.GetUserData());

        const auto client_pid = score::safe_math::Cast<pid_t>(UserDataUintPtr);
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(client_pid.has_value(),
                                     "Message Passing : Message Passing: PID is bigger than pid_t::max()");
        auto executed = (*scoped_function)(client_pid.value(), message);

        if (!executed.has_value())
        {
            score::mw::log::LogInfo("lola") << "MessagePassingServiceInstance: Message callback scope invalidated, "
                                             "skipping message processing for client "
                                          << client_pid.value();
        }

        return {};
    };
    auto received_send_message_with_reply_callback = CreateSendMessageWithReplyCallback();

    // Suppress "AUTOSAR C++14 A5-1-4" rule finding: "A lambda expression object shall not outlive any of its
    // reference-captured objects.".
    // `server_` is deleted in the destructor of the captured `this`
    auto result = server_->StartListening(connect_callback,
                                          disconnect_callback,
                                          // coverity[autosar_cpp14_a5_1_4_violation]
                                          received_send_message_callback,
                                          std::move(received_send_message_with_reply_callback));
    if (!result.has_value())
    {
        score::mw::log::LogFatal("lola") << "MessagePassingService: Failed to start listening on " << service_identifier
                                       << " with following error: " << result.error();
        std::terminate();
    }
}

message_passing::MessageCallback MessagePassingServiceInstance::CreateSendMessageWithReplyCallback()
{
    auto message_callback_with_reply_scoped_function = std::make_shared<
        score::safecpp::MoveOnlyScopedFunction<score::ResultBlank(uid_t, pid_t, score::cpp::span<const std::uint8_t>)>>(
        message_callback_scope_,
        [this](uid_t sender_uid, pid_t sender_pid, score::cpp::span<const std::uint8_t> message) noexcept -> score::ResultBlank {
            return this->MessageCallbackWithReply(sender_uid, sender_pid, message);
        });

    // Note. When received_send_message_with_reply_callback returns an error, the message passing connection with the
    // client will be disconnected. Therefore, we only return an error from the callback when the error is unrecoverable
    // (e.g. an issue with message passing itself). Any recoverable errors are sent back to the client in the reply
    // message and handled there.
    auto received_send_message_with_reply_callback =
        [message_callback_with_reply_scoped_function = std::move(message_callback_with_reply_scoped_function)](
            score::message_passing::IServerConnection& connection,
            score::cpp::span<const std::uint8_t> message) noexcept -> score::cpp::expected_blank<score::os::Error> {
        const auto client_identity = connection.GetClientIdentity();
        const pid_t client_pid = client_identity.pid;
        const auto client_uid = client_identity.uid;

        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(message_callback_with_reply_scoped_function != nullptr,
                                     "Message callback with reply callable was not properly constructed");
        auto function_invocation_result =
            std::invoke(*message_callback_with_reply_scoped_function, client_uid, client_pid, message);
        if (!(function_invocation_result.has_value()))
        {
            score::mw::log::LogError("lola")
                << "Calling message_callback_with_reply_scoped_function failed because scope expired" << client_pid;
            const auto reply = SerializeToMethodReplyMessage(MakeUnexpected(MethodErrc::kSkeletonAlreadyDestroyed));
            const auto reply_result = connection.Reply(reply);
            if (!(reply_result.has_value()))
            {
                score::mw::log::LogError("lola") << "Failed to send reply after failing to process method due to scope "
                                                  "expiring. Disconnecting from client.";
                return score::cpp::make_unexpected(os::Error::createUnspecifiedError());
            }
        }

        const auto& message_handling_result = function_invocation_result.value();
        const auto did_message_handling_fail_unrecoverable =
            !(message_handling_result.has_value()) && (!IsMethodErrorRecoverable(message_handling_result.error()));

        // If we received an unrecoverable error then we log here and return an error after attempting to send a reply.
        // We log since an unrecoverable error may indicate that message passing is broken, so we can't rely on the
        // caller receiving an informative error message. We log here in case the message reply fails which will return
        // early from this function to ensure that we log both error messages in that case.
        if (did_message_handling_fail_unrecoverable)
        {
            score::mw::log::LogError("lola")
                << "Handling message with reply failed with unrecoverable error:" << message_handling_result.error()
                << ". Disconnecting from client.";
        }

        const auto reply = SerializeToMethodReplyMessage(message_handling_result);
        const auto reply_result = connection.Reply(reply);
        if (!(reply_result.has_value()))
        {
            score::mw::log::LogError("lola")
                << "Failed to send reply after successfully processing message with reply. Disconnecting from client.";
            return score::cpp::make_unexpected(os::Error::createUnspecifiedError());
        }

        if (did_message_handling_fail_unrecoverable)
        {
            return score::cpp::make_unexpected(os::Error::createUnspecifiedError());
        }

        return {};
    };
    return received_send_message_with_reply_callback;
}

void MessagePassingServiceInstance::MessageCallback(const pid_t sender_pid,
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

score::ResultBlank MessagePassingServiceInstance::MessageCallbackWithReply(const uid_t sender_uid,
                                                                         const pid_t sender_pid,
                                                                         const score::cpp::span<const std::uint8_t> message)
{
    if (message.size() < 1U)
    {
        score::mw::log::LogError("lola") << "MessagePassingService: Empty message received from " << sender_pid;
        return MakeUnexpected(MethodErrc::kUnexpectedMessageSize);
    }
    const auto payload = message.subspan(1U);
    switch (message.front())
    {
        case score::cpp::to_underlying(MessageWithReplyType::kSubscribeServiceMethod):
        {
            return HandleSubscribeServiceMethodMsg(payload, sender_uid, sender_pid);
        }
        case score::cpp::to_underlying(MessageWithReplyType::kCallMethod):
        {
            return HandleCallMethodMsg(payload, sender_uid);
        }
        default:
        {
            score::mw::log::LogError("lola")
                << "MessagePassingService: Unsupported MessageWithReplyType received from " << sender_pid;
            break;
        }
    }

    return MakeUnexpected(MethodErrc::kUnexpectedMessage);
}

void MessagePassingServiceInstance::HandleNotifyEventMsg(const score::cpp::span<const std::uint8_t> payload,
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

void MessagePassingServiceInstance::HandleRegisterNotificationMsg(const score::cpp::span<const std::uint8_t> payload,
                                                                  const pid_t sender_node_id) noexcept
{
    // TODO: make proper serialization
    ElementFqId elementFqId{};
    if (!DeserializeFromPayload(payload, elementFqId))
    {
        return;
    }

    bool already_registered{false};
    bool notify_status_change = false;

    // Check if there are local handlers first (to maintain lock hierarchy: handlers before nodes)
    std::shared_lock<std::shared_mutex> handlers_read_lock(event_update_handlers_mutex_);
    auto handlers_search = event_update_handlers_.find(elementFqId);
    const bool has_local_handlers =
        (handlers_search != event_update_handlers_.end()) && (!handlers_search->second.empty());
    handlers_read_lock.unlock();

    // TODO: PMR
    std::unique_lock<std::shared_mutex> write_lock(event_update_interested_nodes_mutex_);
    auto search = event_update_interested_nodes_.find(elementFqId);
    if (search != event_update_interested_nodes_.end())
    {
        auto inserted = search->second.insert(sender_node_id);
        already_registered = (inserted.second == false);
        notify_status_change = !already_registered && (search->second.size() == 1U);
    }
    else
    {
        auto emplaced = event_update_interested_nodes_.emplace(elementFqId, std::set<pid_t>{});
        score::cpp::ignore = emplaced.first->second.insert(sender_node_id);
        notify_status_change = true;  // First remote handler
    }
    write_lock.unlock();

    // Only notify if no local handlers exist
    notify_status_change = notify_status_change && !has_local_handlers;

    if (already_registered)
    {
        score::mw::log::LogWarn("lola")
            << "MessagePassingService: Received redundant RegisterEventNotificationMessage for event: "
            << elementFqId.ToString() << " from node " << sender_node_id;
    }

    // Notify SkeletonEvent that the first handler (remote) has been registered.
    // This allows SkeletonEvent to start sending NotifyEvent() calls for this event.
    if (notify_status_change)
    {
        std::shared_lock<std::shared_mutex> callback_read_lock(handler_status_change_callbacks_mutex_);
        auto callback_search = handler_status_change_callbacks_.find(elementFqId);
        if (callback_search != handler_status_change_callbacks_.end())
        {
            callback_search->second(true);  // Now has handlers
        }
    }
}

void MessagePassingServiceInstance::HandleUnregisterNotificationMsg(const score::cpp::span<const std::uint8_t> payload,
                                                                    const pid_t sender_node_id) noexcept
{
    // TODO: make proper serialization
    ElementFqId elementFqId{};
    if (!DeserializeFromPayload(payload, elementFqId))
    {
        return;
    }

    bool registration_found{false};
    bool notify_status_change = false;

    // Check if there are local handlers first (to maintain lock hierarchy: handlers before nodes)
    std::shared_lock<std::shared_mutex> handlers_read_lock(event_update_handlers_mutex_);
    auto handlers_search = event_update_handlers_.find(elementFqId);
    const bool has_local_handlers =
        (handlers_search != event_update_handlers_.end()) && (!handlers_search->second.empty());
    handlers_read_lock.unlock();

    std::unique_lock<std::shared_mutex> write_lock(event_update_interested_nodes_mutex_);
    auto search = event_update_interested_nodes_.find(elementFqId);
    if (search != event_update_interested_nodes_.end())
    {
        registration_found = search->second.erase(sender_node_id) == 1U;

        // If this was the last remote node being unregistered, we have a status change only if there are
        // no local handlers exist.
        if (registration_found && search->second.empty())
        {
            notify_status_change = !has_local_handlers;
        }
    }
    write_lock.unlock();

    if (!registration_found)
    {
        score::mw::log::LogWarn("lola")
            << "MessagePassingService: Received UnregisterEventNotificationMessage for event: "
            << elementFqId.ToString() << " from node " << sender_node_id << ", but there was no registration!";
    }

    // Notify SkeletonEvent that the last handler (remote) has been unregistered.
    // This allows SkeletonEvent to skip NotifyEvent() calls for this event to save performance.
    if (notify_status_change)
    {
        std::shared_lock<std::shared_mutex> callback_read_lock(handler_status_change_callbacks_mutex_);
        auto callback_search = handler_status_change_callbacks_.find(elementFqId);
        if (callback_search != handler_status_change_callbacks_.end())
        {
            callback_search->second(false);  // No handlers remain
        }
    }
}

void MessagePassingServiceInstance::HandleOutdatedNodeIdMsg(const score::cpp::span<const std::uint8_t> payload,
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

score::ResultBlank MessagePassingServiceInstance::HandleSubscribeServiceMethodMsg(
    const score::cpp::span<const std::uint8_t> payload,
    const uid_t sender_uid,
    const pid_t sender_node_id)
{
    SubscribeServiceMethodUnserializedPayload unserialized_payload{};
    if (!DeserializeFromPayload(payload, unserialized_payload))
    {
        return MakeUnexpected(MethodErrc::kUnexpectedMessageSize);
    }

    return CallSubscribeServiceMethodLocally(unserialized_payload.skeleton_instance_identifier,
                                             unserialized_payload.proxy_instance_identifier,
                                             sender_uid,
                                             sender_node_id);
}

score::ResultBlank MessagePassingServiceInstance::HandleCallMethodMsg(const score::cpp::span<const std::uint8_t> payload,
                                                                    const uid_t sender_uid)
{
    // TODO: make proper serialization
    MethodCallUnserializedPayload unserialized_payload{};
    if (!DeserializeFromPayload(payload, unserialized_payload))
    {
        return MakeUnexpected(MethodErrc::kUnexpectedMessageSize);
    }

    return CallServiceMethodLocally(
        unserialized_payload.proxy_method_instance_identifier, unserialized_payload.queue_position, sender_uid);
}

score::ResultBlank MessagePassingServiceInstance::CallSubscribeServiceMethodLocally(
    const SkeletonInstanceIdentifier& skeleton_instance_identifier,
    const ProxyInstanceIdentifier& proxy_instance_identifier,
    const uid_t proxy_uid,
    const pid_t proxy_pid)
{
    // A copy of the handler is made under lock and called outside the lock to allow calling multiple handlers at once
    // and to also allow registering a new method call handler for the same ProxyInstanceIdentifier while an old
    // call is still running. The handler is the map may also be unregistered once the mutex is released (while the copy
    // of the handler still exists). The handler will still be called in this case unless the handler's scope has
    // expired. The scope of the handler ensures. that it doesn't access any expired resources.
    std::shared_lock<std::shared_mutex> read_lock{subscribe_service_method_handlers_mutex_};
    auto method_subscribed_handler_it = subscribe_service_method_handlers_.find(skeleton_instance_identifier);
    if (method_subscribed_handler_it == subscribe_service_method_handlers_.cend())
    {
        // This can occur if a ProxyMethod calls subscribe method with an invalid/corrupted SkeletonInstanceIdentifier.
        mw::log::LogError("lola") << "Subscribe method handler has not been registered for this SkeletonMethod!";
        return MakeUnexpected(MethodErrc::kNotOffered);
    }

    auto [method_subscribed_handler_copy, allowed_proxy_uids] = method_subscribed_handler_it->second;
    read_lock.unlock();

    if (allowed_proxy_uids.has_value() && allowed_proxy_uids->count(proxy_uid) == 0U)
    {
        mw::log::LogError("lola") << "Could not invoke subscribe service method handler because uid of proxy calling "
                                     "subscribe is not in allowed_consumers list.";
        return MakeUnexpected(MethodErrc::kUnknownProxy);
    }
    const auto invocation_result =
        std::invoke(method_subscribed_handler_copy, proxy_instance_identifier, proxy_uid, proxy_pid);
    if (!(invocation_result.has_value()))
    {
        mw::log::LogError("lola")
            << "Invocation of subscribe service method handler failed as scope has been destroyed: "
               "SkeletonMethod has already been destroyed.";
        return MakeUnexpected(MethodErrc::kSkeletonAlreadyDestroyed);
    }
    return invocation_result.value();
}

score::ResultBlank MessagePassingServiceInstance::CallServiceMethodLocally(
    const ProxyMethodInstanceIdentifier& proxy_method_instance_identifier,
    const std::size_t queue_position,
    const uid_t proxy_uid)
{
    // A copy of the handler is made under lock and called outside the lock to allow calling multiple handlers at once
    // and to also allow registering a new method call handler for the same ProxyInstanceIdentifier while an old
    // call is still running. The handler is the map may also be unregistered once the mutex is released (while the copy
    // of the handler still exists). The handler will still be called in this case unless the handler's scope has
    // expired. The scope of the handler ensures. that it doesn't access any expired resources.
    std::shared_lock<std::shared_mutex> read_lock{call_method_handlers_mutex_};
    auto method_call_handler_it = call_method_handlers_.find(proxy_method_instance_identifier);
    if (method_call_handler_it == call_method_handlers_.cend())
    {
        // This can occur if calling a method when the skeleton has crashed and restarted but the proxy hasn't yet
        // re-subscribed (so the method call handler has not yet been registered). It can also occur if a ProxyMethod
        // calls the method with an invalid/corrupted ProxyMethodInstanceIdentifier.
        mw::log::LogError("lola") << "Method call handler has not been registered for this ProxyMethod!";
        return MakeUnexpected(MethodErrc::kNotSubscribed);
    }

    auto [method_call_handler_copy, allowed_proxy_uid] = method_call_handler_it->second;
    read_lock.unlock();

    if (allowed_proxy_uid != proxy_uid)
    {
        mw::log::LogError("lola") << "Could not invoke method call handler because uid of proxy calling method is "
                                     "not the same one that registered the handler.";
        return MakeUnexpected(MethodErrc::kUnknownProxy);
    }
    auto invocation_result = std::invoke(method_call_handler_copy, queue_position);
    if (!(invocation_result.has_value()))
    {
        mw::log::LogError("lola") << "Invocation of method call handler failed as scope has been destroyed: "
                                     "SkeletonMethod has already been destroyed.";
        return MakeUnexpected(MethodErrc::kSkeletonAlreadyDestroyed);
    }
    return {};
}

ResultBlank MessagePassingServiceInstance::CallSubscribeServiceMethodRemotely(
    const SkeletonInstanceIdentifier& skeleton_instance_identifier,
    const ProxyInstanceIdentifier& proxy_instance_identifier,
    const pid_t target_node_id)
{
    SubscribeServiceMethodUnserializedPayload unserialized_payload{skeleton_instance_identifier,
                                                                   proxy_instance_identifier};
    const auto message =
        SerializeToMessage(score::cpp::to_underlying(MessageWithReplyType::kSubscribeServiceMethod), unserialized_payload);
    auto sender = client_cache_.GetMessagePassingClient(target_node_id);

    std::array<std::uint8_t, sizeof(MethodReplyPayload)> reply{};
    score::cpp::span<std::uint8_t> reply_buffer{reply.data(), reply.size()};
    const auto method_reply_result = sender->SendWaitReply(message, reply_buffer);
    if (!method_reply_result.has_value())
    {
        score::mw::log::LogError("lola")
            << "MessagePassingServiceInstance: Sending SubscribeServiceMethodMessage to node_id " << target_node_id
            << " failed with error: " << method_reply_result.error();
        return MakeUnexpected(MethodErrc::kMessagePassingError);
    }

    const auto deserialization_result = DeserializeFromMethodReplyPayload(method_reply_result.value());
    if (!(deserialization_result.has_value()))
    {
        score::mw::log::LogError("lola")
            << "MessagePassingService: Parsing SubscribeServiceMethodMessage reply from node_id " << target_node_id
            << "failed during deserialization";
        return MakeUnexpected<Blank>(deserialization_result.error());
    }
    return deserialization_result.value();
}

ResultBlank MessagePassingServiceInstance::CallServiceMethodRemotely(
    const ProxyMethodInstanceIdentifier& proxy_method_instance_identifier,
    const std::size_t queue_position,
    const pid_t target_node_id)
{
    const MethodCallUnserializedPayload unserialized_payload{proxy_method_instance_identifier, queue_position};
    const auto message =
        SerializeToMessage(score::cpp::to_underlying(MessageWithReplyType::kCallMethod), unserialized_payload);
    auto sender = client_cache_.GetMessagePassingClient(target_node_id);

    std::array<std::uint8_t, sizeof(MethodReplyPayload)> reply{};
    score::cpp::span<std::uint8_t> reply_buffer{reply.data(), reply.size()};
    const auto send_wait_reply_result = sender->SendWaitReply(message, reply_buffer);
    if (!(send_wait_reply_result.has_value()))
    {
        score::mw::log::LogError("lola") << "MessagePassingService: Sending CallServiceMethodMessage to node_id "
                                       << target_node_id << " failed with error: " << send_wait_reply_result.error();
        return MakeUnexpected(MethodErrc::kMessagePassingError);
    }
    const auto reply_payload = send_wait_reply_result.value();

    const auto method_call_deserialization_result = DeserializeFromMethodReplyPayload(reply_payload);
    if (!(method_call_deserialization_result.has_value()))
    {
        score::mw::log::LogError("lola") << "MessagePassingService: Parsing CallServiceMethodMessage reply from node_id "
                                       << target_node_id << "failed during deserialization";
        return MakeUnexpected(MethodErrc::kUnexpectedMessageSize);
    }
    const auto method_call_result = method_call_deserialization_result.value();

    if (!(method_call_result.has_value()))
    {
        score::mw::log::LogError("lola") << "MessagePassingService: CallServiceMethodMessage reply from node_id "
                                       << target_node_id << "returned failure";
        return MakeUnexpected<Blank>(method_call_result.error());
    }
    return {};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly".
// This is a false positive: .at() could throw if the index is outside of the range of the container but the function
// CopyNodeIdentifiers will only return a value which is in range of the array. Otherwise CopyNodeIdentifiers will break
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
void MessagePassingServiceInstance::NotifyEventRemote(const ElementFqId event_id) noexcept
{
    NodeIdTmpBufferType nodeIdentifiersTmp;
    pid_t start_node_id{0};
    const auto message = SerializeToMessage(score::cpp::to_underlying(MessageType::kNotifyEvent), event_id);
    std::pair<std::uint8_t, bool> num_ids_copied;
    std::uint8_t loop_count{0U};
    do
    {
        // LCOV_EXCL_START exceeds "short" limit for unit tests
        if (loop_count == 255U)
        {
            score::mw::log::LogError("lola") << "An overflow in counting the node identifiers to notifies event update.";
            break;
        }
        // LCOV_EXCL_STOP
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
                score::mw::log::LogError("lola") << "MessagePassingService: Sending NotifyEventUpdateMessage to node_id "
                                               << node_identifier << " failed with error: " << result.error();
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

std::uint32_t MessagePassingServiceInstance::NotifyEventLocally(const ElementFqId event_id) noexcept
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
    // LCOV_EXCL_START: decision couldn't be analyzed; considered normal under 100% line coverage
    while ((handler_it != handlers_for_event.cend()) && (number_weak_ptrs_copied < kMaxReceiveHandlersPerEvent))
    // LCOV_EXCL_STOP
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

void MessagePassingServiceInstance::NotifyEvent(const ElementFqId event_id) noexcept
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
        // Suppress "AUTOSAR C++14 A15-4-2" rule finding. This rule states: "If a function is declared to be noexcept,
        // noexcept(true) or noexcept(<true condition>), then it shall not exit with an exception.". the function Post
        // throws on allocation failure but this throw directly leads to a termination based on a compiler hook.
        // and the whole function scope doesn't lead to any exception.
        // coverity[autosar_cpp14_a15_4_2_violation]
        executor_.Post(
            [this](const score::cpp::stop_token& /*token*/, const ElementFqId element_id) noexcept {
                // ignoring the result (number of actually notified local proxy-events),
                // as we don't have any expectation, how many are there.
                score::cpp::ignore = this->NotifyEventLocally(element_id);
            },
            event_id);
    }
}

IMessagePassingService::HandlerRegistrationNoType MessagePassingServiceInstance::RegisterEventNotification(
    const ElementFqId event_id,
    std::weak_ptr<ScopedEventReceiveHandler> callback,
    const pid_t target_node_id) noexcept
{
    bool notify_status_change = false;

    std::unique_lock<std::shared_mutex> write_lock(event_update_handlers_mutex_);

    // Check if this is the first handler being registered
    auto search = event_update_handlers_.find(event_id);
    const bool was_empty = (search == event_update_handlers_.end()) || (search->second.empty());

    // The rule against increment and decrement operators mixed with others on the same line sems to be more applicable
    // to arithmetic operators. Here the only other operator is the member access operator (i.e. the dot operator) and
    // it in no way confusing to combine it with the increment operator.
    // This rule is also absent from the new MISRA 2023 standard, thus it is in general reasonable to deviate from it.
    // coverity[autosar_cpp14_m5_2_10_violation]
    const IMessagePassingService::HandlerRegistrationNoType registration_no = cur_registration_no_++;
    RegisteredNotificationHandler newHandler{std::move(callback), registration_no};
    if (search != event_update_handlers_.end())
    {
        search->second.push_back(std::move(newHandler));
    }
    else
    {
        auto result = event_update_handlers_.emplace(event_id, std::vector<RegisteredNotificationHandler>{});
        result.first->second.push_back(std::move(newHandler));
    }

    // Check if we need to notify about status change (transition from 0 to 1 local handlers)
    // We only notify if there were no remote handlers either
    if (was_empty)
    {
        std::shared_lock<std::shared_mutex> nodes_read_lock(event_update_interested_nodes_mutex_);
        auto nodes_search = event_update_interested_nodes_.find(event_id);
        const bool has_remote_handlers =
            (nodes_search != event_update_interested_nodes_.end()) && (!nodes_search->second.empty());
        nodes_read_lock.unlock();
        // Only notify if no remote handlers exist, because only then we have a state change from 0 -> >0.
        notify_status_change = !has_remote_handlers;
    }

    write_lock.unlock();

    if (target_node_id != self_pid_)
    {
        RegisterEventNotificationRemote(event_id, target_node_id);
    }

    // Notify SkeletonEvent that the first handler (local) has been registered.
    // This allows SkeletonEvent to start sending NotifyEvent() calls for this event.
    if (notify_status_change)
    {
        std::shared_lock<std::shared_mutex> callback_read_lock(handler_status_change_callbacks_mutex_);
        auto callback_search = handler_status_change_callbacks_.find(event_id);
        if (callback_search != handler_status_change_callbacks_.end())
        {
            callback_search->second(true);  // Now has handlers
        }
    }

    return registration_no;
}

void MessagePassingServiceInstance::ReregisterEventNotification(const ElementFqId event_id,
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
    const auto is_target_remote_node = (target_node_id != self_pid_);
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

            // The rule against increment and decrement operators mixed with others on the same line sems to be more
            // applicable to arithmetic operators. Here the only other operator is the member access operator (i.e. the
            // dot operator) and it in no way confusing to combine it with the increment operator. This rule is also
            // absent from the new MISRA 2023 standard, thus it is in general reasonable to deviate from it.
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

void MessagePassingServiceInstance::UnregisterEventNotification(
    const ElementFqId event_id,
    const IMessagePassingService::HandlerRegistrationNoType registration_no,
    const pid_t target_node_id) noexcept
{
    bool found{false};
    bool notify_status_change = false;

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

            // If this was the last local handler, check if remote nodes interested in update notification exist
            if (search->second.empty())
            {
                std::shared_lock<std::shared_mutex> nodes_read_lock(event_update_interested_nodes_mutex_);
                auto nodes_search = event_update_interested_nodes_.find(event_id);
                const bool has_remote_handlers =
                    (nodes_search != event_update_interested_nodes_.end()) && (!nodes_search->second.empty());
                nodes_read_lock.unlock();
                // Only notify if no remote handlers exist, because only then we have a state change from >0 -> 0.
                notify_status_change = !has_remote_handlers;
            }
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

    if (target_node_id != self_pid_)
    {
        UnregisterEventNotificationRemote(event_id, registration_no, target_node_id);
    }

    // Notify SkeletonEvent that the last handler (local) has been unregistered.
    // This allows SkeletonEvent to skip NotifyEvent() calls for this event to save performance.
    if (notify_status_change)
    {
        std::shared_lock<std::shared_mutex> callback_read_lock(handler_status_change_callbacks_mutex_);
        auto callback_search = handler_status_change_callbacks_.find(event_id);
        if (callback_search != handler_status_change_callbacks_.end())
        {
            callback_search->second(false);  // No handlers remain
        }
    }
}

ResultBlank MessagePassingServiceInstance::RegisterOnServiceMethodSubscribedHandler(
    const SkeletonInstanceIdentifier skeleton_instance_identifier,
    IMessagePassingService::ServiceMethodSubscribedHandler subscribed_callback,
    IMessagePassingService::AllowedConsumerUids allowed_proxy_uids)
{
    std::unique_lock<std::shared_mutex> write_lock(subscribe_service_method_handlers_mutex_);
    const auto [existing_element, was_inserted] = subscribe_service_method_handlers_.insert(
        {skeleton_instance_identifier, {std::move(subscribed_callback), std::move(allowed_proxy_uids)}});

    if (!was_inserted)
    {
        score::mw::log::LogError("lola") << "MessagePassingService: Failed to register OnServiceMethodSubscribedHandler "
                                          "since it could not be inserted into map.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }

    return {};
}

ResultBlank MessagePassingServiceInstance::RegisterMethodCallHandler(
    const ProxyMethodInstanceIdentifier proxy_method_instance_identifier,
    IMessagePassingService::MethodCallHandler method_call_callback,
    const uid_t allowed_proxy_uid)
{
    std::unique_lock<std::shared_mutex> write_lock(call_method_handlers_mutex_);

    /// TODO: Add in a comment explaining that we need to overwrite handlers here in case the Proxy has restarted and
    /// needs to register NEW method call handlers with pointers in the NEW shared memory region.
    const auto handler_it = call_method_handlers_.find(proxy_method_instance_identifier);
    if (handler_it == call_method_handlers_.cend())
    {
        score::cpp::ignore = call_method_handlers_.insert(
            {proxy_method_instance_identifier, {std::move(method_call_callback), allowed_proxy_uid}});
    }
    else
    {
        handler_it->second.first = std::move(method_call_callback);
        handler_it->second.second = allowed_proxy_uid;
    }

    return {};
}

void MessagePassingServiceInstance::UnregisterOnServiceMethodSubscribedHandler(
    const SkeletonInstanceIdentifier skeleton_instance_identifier)
{
    std::unique_lock<std::shared_mutex> write_lock(subscribe_service_method_handlers_mutex_);
    const auto num_elements_erased = subscribe_service_method_handlers_.erase(skeleton_instance_identifier);
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        num_elements_erased != 0U,
        "Function must only be called when a subscribe service method handler was successfully registered!");
}

void MessagePassingServiceInstance::UnregisterMethodCallHandler(
    const ProxyMethodInstanceIdentifier proxy_method_instance_identifier)
{
    std::unique_lock<std::shared_mutex> write_lock(call_method_handlers_mutex_);
    const auto num_elements_erased = call_method_handlers_.erase(proxy_method_instance_identifier);
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        num_elements_erased != 0U,
        "Function must only be called when a method call handler was successfully registered!");
}

void MessagePassingServiceInstance::NotifyOutdatedNodeId(const pid_t outdated_node_id,
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

void MessagePassingServiceInstance::RegisterEventNotificationRemote(const ElementFqId event_id,
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
            // The rule against increment and decrement operators mixed with others on the same line sems to be more
            // applicable to arithmetic operators. Here the only other operator is the member access operator (i.e. the
            // dot operator) and it in no way confusing to combine it with the increment operator. This rule is also
            // absent from the new MISRA 2023 standard, thus it is in general reasonable to deviate from it.
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

void MessagePassingServiceInstance::UnregisterEventNotificationRemote(
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
        score::mw::log::LogError("lola") << "MessagePassingService: UnregisterEventNotification called with register_no "
                                       << registration_no << " for a remote event " << event_id.ToString()
                                       << " without current remote registration!";
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

        // The rule against increment and decrement operators mixed with others on the same line sems to be more
        // applicable to arithmetic operators. Here the only other operator is the member access operator (i.e. the dot
        // operator) and it in no way confusing to combine it with the decrement operator. This rule is also absent from
        // the new MISRA 2023 standard, thus it is in general reasonable to deviate from it.
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
        const auto message = SerializeToMessage(score::cpp::to_underlying(MessageType::kUnregisterEventNotifier), event_id);
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

void MessagePassingServiceInstance::SendRegisterEventNotificationMessage(const ElementFqId event_id,
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
        score::mw::log::LogError("lola") << "MessagePassingService: Sending RegisterEventNotificationMessage to node_id "
                                       << target_node_id << " failed with error: " << result.error();
    }
}

void MessagePassingServiceInstance::RegisterEventNotificationExistenceChangedCallback(
    const ElementFqId event_id,
    IMessagePassingService::HandlerStatusChangeCallback callback) noexcept
{
    std::unique_lock<std::shared_mutex> write_lock(handler_status_change_callbacks_mutex_);
    handler_status_change_callbacks_[event_id] = std::move(callback);
    write_lock.unlock();

    // Check current handler status and invoke callback if handlers already exist
    std::shared_lock<std::shared_mutex> handlers_read_lock(event_update_handlers_mutex_);
    auto handlers_search = event_update_handlers_.find(event_id);
    const bool has_local_handlers =
        (handlers_search != event_update_handlers_.end()) && (!handlers_search->second.empty());
    handlers_read_lock.unlock();

    std::shared_lock<std::shared_mutex> nodes_read_lock(event_update_interested_nodes_mutex_);
    auto nodes_search = event_update_interested_nodes_.find(event_id);
    const bool has_remote_handlers =
        (nodes_search != event_update_interested_nodes_.end()) && (!nodes_search->second.empty());
    nodes_read_lock.unlock();

    const bool has_any_handlers = has_local_handlers || has_remote_handlers;

    // Invoke the callback immediately only if handlers are already registered.
    // This avoids unnecessary callback invocation when the atomic flags are already initialized to false.
    if (has_any_handlers)
    {
        std::shared_lock<std::shared_mutex> callback_read_lock(handler_status_change_callbacks_mutex_);
        auto callback_search = handler_status_change_callbacks_.find(event_id);

        // LCOV_EXCL_BR_START: Defensive programming: false branch is unreachable in practice. The code inserts a
        // callback into the map, then immediately looks it up. Between these two operations (both protected by
        // mutexes), there's no way for the callback to be removed.
        if (callback_search != handler_status_change_callbacks_.end())
        // LCOV_EXCL_BR_STOP
        {
            callback_search->second(true);
        }
    }
}

void MessagePassingServiceInstance::UnregisterEventNotificationExistenceChangedCallback(
    const ElementFqId event_id) noexcept
{
    std::unique_lock<std::shared_mutex> write_lock(handler_status_change_callbacks_mutex_);
    std::size_t erased_count = handler_status_change_callbacks_.erase(event_id);
    write_lock.unlock();

    if (erased_count == 0U)
    {
        score::mw::log::LogWarn("lola")
            << "MessagePassingService: UnregisterEventNotificationExistenceChangedCallback called for event "
            << event_id.ToString() << " but no callback was registered";
    }
}

QualityType MessagePassingServiceInstance::GetPartnerQualityType() const
{
    switch (asil_level_)
    {
        case ClientQualityType::kASIL_QM:
            return QualityType::kASIL_QM;
        case ClientQualityType::kASIL_B:
            return QualityType::kASIL_B;
        case ClientQualityType::kASIL_QMfromB:
            return QualityType::kASIL_QM;
        default:
            std::terminate();
    }
}

ResultBlank MessagePassingServiceInstance::SubscribeServiceMethod(
    const SkeletonInstanceIdentifier& skeleton_instance_identifier,
    const ProxyInstanceIdentifier& proxy_instance_identifier,
    const pid_t target_node_id)
{
    const auto are_skeleton_and_proxy_in_same_process = (target_node_id == self_pid_);
    if (are_skeleton_and_proxy_in_same_process)
    {
        const auto result = CallSubscribeServiceMethodLocally(
            skeleton_instance_identifier, proxy_instance_identifier, self_uid_, target_node_id);
        if (!(result.has_value()))
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
        return {};
    }
    else
    {
        const auto result =
            CallSubscribeServiceMethodRemotely(skeleton_instance_identifier, proxy_instance_identifier, target_node_id);
        if (!(result.has_value()))
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
        return {};
    }
}

ResultBlank MessagePassingServiceInstance::CallMethod(
    const ProxyMethodInstanceIdentifier& proxy_method_instance_identifier,
    std::size_t queue_position,
    const pid_t target_node_id)
{
    const auto are_skeleton_and_proxy_in_same_process = (target_node_id == self_pid_);
    if (are_skeleton_and_proxy_in_same_process)
    {
        const auto result = CallServiceMethodLocally(proxy_method_instance_identifier, queue_position, self_uid_);
        if (!(result.has_value()))
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
        return {};
    }
    else
    {
        const auto result = CallServiceMethodRemotely(proxy_method_instance_identifier, queue_position, target_node_id);
        if (!(result.has_value()))
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
        return {};
    }
}

}  // namespace score::mw::com::impl::lola
