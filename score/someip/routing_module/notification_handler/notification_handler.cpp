/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/someip/routing_module/notification_handler/notification_handler.h"

#include "score/someip/routing_module/iosock_transport/iosock_transport.h"
#include "score/someip/routing_module/lola_mapping/lola_mapping.h"

#include "score/mw/log/logging.h"

#include <score/span.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace score::someip::routing_module
{
namespace
{

constexpr auto kLogContext{"SRTM"};

bool SameEndpoint(const Endpoint& lhs, const Endpoint& rhs) noexcept
{
    return (lhs.address == rhs.address) && (lhs.port == rhs.port);
}

void LogDroppedNotification(score::mw::log::LogStream log,
                            const std::string_view reason,
                            const SomeipMessageHeader& header,
                            const std::uint16_t someip_instance) noexcept
{
    log << "Notification of service" << score::mw::log::LogHex16{header.service_id} << "instance"
        << score::mw::log::LogHex16{someip_instance} << "event" << score::mw::log::LogHex16{header.method_or_event_id}
        << "with" << header.payload_size << "payload bytes dropped," << reason;
}

}  // namespace

NotificationHandler::NotificationHandler() noexcept = default;

NotificationHandler::~NotificationHandler() noexcept
{
    // Destroying an offered LoLa skeleton does not stop its LoLa offer.
    StopAllReceivePaths();
}

CommandResult NotificationHandler::AddRemoteService(const routing_control_protocol::Message& command) noexcept
{
    if ((command.local_port == 0U) || (command.remote_port == 0U))
    {
        score::mw::log::LogWarn(kLogContext)
            << "Remote service with UDP port 0 rejected, service" << score::mw::log::LogHex16{command.service}
            << "instance" << score::mw::log::LogHex16{command.instance};
        return CommandResult::kRejected;
    }
    if (UdpRouteUsedByOtherInstance(command))
    {
        score::mw::log::LogWarn(kLogContext)
            << "Remote service rejected, its UDP route is used by another instance; service"
            << score::mw::log::LogHex16{command.service} << "instance" << score::mw::log::LogHex16{command.instance};
        return CommandResult::kRejected;
    }
    const int udp_open_error = someip_iosock_open_udp_socket(command.local_port);
    if (udp_open_error != 0)
    {
        score::mw::log::LogError(kLogContext)
            << "Opening UDP port" << command.local_port << "failed with error" << udp_open_error;
        return CommandResult::kRejected;
    }
    const SomeipServiceInstance key{command.service, command.instance};
    auto entry = remote_services_.find(key);
    if (entry == remote_services_.end())
    {
        auto created = CreateLolaSkeleton(command);
        if (!created.has_value())
        {
            return CommandResult::kRejected;
        }
        entry = remote_services_.emplace(key, std::move(created).value()).first;
    }
    RemoteService& remote_service = entry->second;
    if (!remote_service.lola_offered)
    {
        if (!remote_service.lola_skeleton->OfferService().has_value())
        {
            score::mw::log::LogError(kLogContext)
                << "Offering the LoLa service instance failed, service" << score::mw::log::LogHex16{command.service}
                << "instance" << score::mw::log::LogHex16{command.instance};
            return CommandResult::kRejected;
        }
        remote_service.lola_offered = true;
    }
    remote_service.udp_local_port = command.local_port;
    remote_service.udp_remote_endpoint = Endpoint{command.remote_ip, command.remote_port};
    return CommandResult::kApplied;
}

CommandResult NotificationHandler::RemoveRemoteService(const ServiceInstance service_instance) noexcept
{
    const auto entry =
        remote_services_.find(SomeipServiceInstance{service_instance.service, service_instance.instance});
    if (entry == remote_services_.end())
    {
        return CommandResult::kRejected;
    }
    StopReceivePath(entry->second);
    return CommandResult::kApplied;
}

CommandResult NotificationHandler::AddLocalSubscription(const ServiceInstance service_instance,
                                                        const std::uint16_t event) noexcept
{
    RemoteEvent* const remote_event = FindLolaOfferedEvent(service_instance, event);
    if (remote_event == nullptr)
    {
        return CommandResult::kRejected;
    }
    remote_event->local_subscription = true;
    return CommandResult::kApplied;
}

CommandResult NotificationHandler::RemoveLocalSubscription(const ServiceInstance service_instance,
                                                           const std::uint16_t event) noexcept
{
    RemoteEvent* const remote_event = FindLolaOfferedEvent(service_instance, event);
    if (remote_event == nullptr)
    {
        return CommandResult::kRejected;
    }
    remote_event->local_subscription = false;
    return CommandResult::kApplied;
}

// The TX direction is not supported yet.
CommandResult NotificationHandler::AddLocalService(const routing_control_protocol::Message& /*command*/) noexcept
{
    return CommandResult::kRejected;
}

CommandResult NotificationHandler::RemoveLocalService(const ServiceInstance /*service_instance*/) noexcept
{
    return CommandResult::kRejected;
}

CommandResult NotificationHandler::AddRemoteSubscription(const ServiceInstance /*service_instance*/,
                                                         const std::uint16_t /*event*/,
                                                         const Endpoint /*subscriber*/) noexcept
{
    return CommandResult::kRejected;
}

CommandResult NotificationHandler::RemoveRemoteSubscription(const ServiceInstance /*service_instance*/,
                                                            const std::uint16_t /*event*/,
                                                            const Endpoint /*subscriber*/) noexcept
{
    return CommandResult::kRejected;
}

void NotificationHandler::RemoveAll() noexcept
{
    StopAllReceivePaths();
}

void NotificationHandler::OnUdpDatagram(const std::uint16_t udp_local_port,
                                        const Endpoint& udp_sender,
                                        const IoSockPacketReader& datagram) noexcept
{
    SomeipDatagramParser parser{};
    auto message = parser.NextMessage(datagram);
    while (message.has_value() && message.value().has_value())
    {
        const SomeipMessageFrame& frame = message.value().value();
        if (frame.header.message_type == kSomeipNotificationMessageType)
        {
            ForwardToLola(udp_local_port, udp_sender, frame, datagram);
        }
        else
        {
            score::mw::log::LogWarn(kLogContext)
                << "SOME/IP message of service" << score::mw::log::LogHex16{frame.header.service_id} << "method"
                << score::mw::log::LogHex16{frame.header.method_or_event_id} << "with message type"
                << score::mw::log::LogHex8{frame.header.message_type}
                << "dropped, only notifications are routed; UDP port" << udp_local_port << "sender"
                << udp_sender.address[0U] << udp_sender.address[1U] << udp_sender.address[2U] << udp_sender.address[3U]
                << "port" << udp_sender.port;
        }
        message = parser.NextMessage(datagram);
    }
    if (!message.has_value())
    {
        score::mw::log::LogWarn(kLogContext) << "Rest of a malformed SOME/IP datagram on UDP port" << udp_local_port
                                             << "dropped, parse error" << static_cast<std::uint32_t>(message.error());
    }
}

std::optional<NotificationHandler::RemoteService> NotificationHandler::CreateLolaSkeleton(
    const routing_control_protocol::Message& command) noexcept
{
    auto lola_specifier =
        score::mw::com::InstanceSpecifier::Create(LolaInstanceSpecifier(command.service, command.instance));
    if (!lola_specifier.has_value())
    {
        score::mw::log::LogError(kLogContext)
            << "Invalid LoLa instance specifier for service" << score::mw::log::LogHex16{command.service} << "instance"
            << score::mw::log::LogHex16{command.instance};
        return std::nullopt;
    }
    std::vector<std::string> lola_event_names{};
    lola_event_names.reserve(command.events.size());
    for (const std::uint16_t someip_event : command.events)
    {
        lola_event_names.push_back(LolaEventName(someip_event));
    }
    std::vector<score::mw::com::EventInfo> lola_event_infos{};
    lola_event_infos.reserve(lola_event_names.size());
    for (const std::string& lola_event_name : lola_event_names)
    {
        lola_event_infos.push_back(score::mw::com::EventInfo{
            lola_event_name, score::mw::com::DataTypeMetaInfo{sizeof(EventSample), alignof(EventSample)}});
    }
    score::mw::com::GenericSkeletonServiceElementInfo lola_elements{};
    lola_elements.events =
        score::cpp::span<const score::mw::com::EventInfo>{lola_event_infos.data(), lola_event_infos.size()};
    auto lola_skeleton = score::mw::com::GenericSkeleton::Create(lola_specifier.value(), lola_elements);
    if (!lola_skeleton.has_value())
    {
        score::mw::log::LogError(kLogContext)
            << "Creating the LoLa skeleton failed, is it missing in mw_com_config.json? Service"
            << score::mw::log::LogHex16{command.service} << "instance" << score::mw::log::LogHex16{command.instance};
        return std::nullopt;
    }
    RemoteService remote_service{
        0U, {}, std::make_unique<score::mw::com::GenericSkeleton>(std::move(lola_skeleton).value()), false, {}};
    auto lola_events = remote_service.lola_skeleton->GetEvents();
    for (const std::uint16_t someip_event : command.events)
    {
        const auto lola_event = lola_events.find(LolaEventName(someip_event));
        if (lola_event != lola_events.end())
        {
            remote_service.someip_events.emplace(someip_event, RemoteEvent{&lola_event->second, false});
        }
    }
    return remote_service;
}

bool NotificationHandler::UdpRouteUsedByOtherInstance(const routing_control_protocol::Message& command) noexcept
{
    // A notification carries no instance ID, so service ID, local UDP port and sender must identify one instance.
    const auto receive_path =
        FindReceivePath(command.service, command.local_port, Endpoint{command.remote_ip, command.remote_port});
    return (receive_path != remote_services_.end()) && (receive_path->first.second != command.instance);
}

NotificationHandler::RemoteServices::iterator NotificationHandler::FindReceivePath(const std::uint16_t someip_service,
                                                                                   const std::uint16_t udp_local_port,
                                                                                   const Endpoint& udp_sender) noexcept
{
    for (auto entry = remote_services_.lower_bound(SomeipServiceInstance{someip_service, 0U});
         (entry != remote_services_.end()) && (entry->first.first == someip_service);
         ++entry)
    {
        const RemoteService& remote_service = entry->second;
        if (remote_service.lola_offered && (remote_service.udp_local_port == udp_local_port) &&
            SameEndpoint(remote_service.udp_remote_endpoint, udp_sender))
        {
            return entry;
        }
    }
    return remote_services_.end();
}

NotificationHandler::RemoteEvent* NotificationHandler::FindLolaOfferedEvent(const ServiceInstance service_instance,
                                                                            const std::uint16_t event) noexcept
{
    const auto entry =
        remote_services_.find(SomeipServiceInstance{service_instance.service, service_instance.instance});
    if ((entry == remote_services_.end()) || (!entry->second.lola_offered))
    {
        return nullptr;
    }
    const auto remote_event = entry->second.someip_events.find(event);
    return (remote_event != entry->second.someip_events.end()) ? &remote_event->second : nullptr;
}

void NotificationHandler::ForwardToLola(const std::uint16_t udp_local_port,
                                        const Endpoint& udp_sender,
                                        const SomeipMessageFrame& notification,
                                        const IoSockPacketReader& datagram) noexcept
{
    const SomeipMessageHeader& header = notification.header;
    const auto receive_path = FindReceivePath(header.service_id, udp_local_port, udp_sender);
    if (receive_path == remote_services_.end())
    {
        score::mw::log::LogWarn(kLogContext)
            << "Notification of service" << score::mw::log::LogHex16{header.service_id} << "event"
            << score::mw::log::LogHex16{header.method_or_event_id} << "dropped, no active receive path for UDP port"
            << udp_local_port << "and sender" << udp_sender.address[0U] << udp_sender.address[1U]
            << udp_sender.address[2U] << udp_sender.address[3U] << "port" << udp_sender.port;
        return;
    }
    const std::uint16_t someip_instance = receive_path->first.second;
    const RemoteService& remote_service = receive_path->second;
    const auto remote_event = remote_service.someip_events.find(header.method_or_event_id);
    if (remote_event == remote_service.someip_events.end())
    {
        LogDroppedNotification(
            score::mw::log::LogWarn(kLogContext), "event unknown to the remote service", header, someip_instance);
        return;
    }
    if (!remote_event->second.local_subscription)
    {
        LogDroppedNotification(
            score::mw::log::LogWarn(kLogContext), "event not locally subscribed", header, someip_instance);
        return;
    }
    // The drops below only happen in a misconfigured or faulty system.
    if (header.payload_size > kMaxPayloadSize)
    {
        LogDroppedNotification(
            score::mw::log::LogError(kLogContext), "payload exceeds the LoLa sample", header, someip_instance);
        return;
    }
    score::mw::com::GenericSkeletonEvent& lola_event = *remote_event->second.lola_event;
    auto lola_sample = lola_event.Allocate();
    if (!lola_sample.has_value())
    {
        LogDroppedNotification(
            score::mw::log::LogError(kLogContext), "no free LoLa sample slot", header, someip_instance);
        return;
    }
    auto* const event_sample = static_cast<EventSample*>(lola_sample.value().Get());
    event_sample->size = static_cast<std::uint32_t>(header.payload_size);
    event_sample->session = header.session_id;
    event_sample->interface_version = header.interface_version;
    event_sample->message_type = header.message_type;
    // The only payload copy of the receive path: from the io-sock packet directly into the LoLa sample slot.
    if (!datagram.Copy(notification.payload_offset,
                       score::cpp::span<std::uint8_t>{event_sample->payload.data(), header.payload_size}))
    {
        LogDroppedNotification(score::mw::log::LogError(kLogContext),
                               "copying the payload from the io-sock packet failed",
                               header,
                               someip_instance);
        return;
    }
    if (!lola_event.Send(std::move(lola_sample).value()).has_value())
    {
        LogDroppedNotification(
            score::mw::log::LogError(kLogContext), "sending the LoLa sample failed", header, someip_instance);
    }
}

void NotificationHandler::StopReceivePath(RemoteService& remote_service) noexcept
{
    if (remote_service.lola_offered)
    {
        remote_service.lola_skeleton->StopOfferService();
        remote_service.lola_offered = false;
    }
    for (auto& someip_event : remote_service.someip_events)
    {
        someip_event.second.local_subscription = false;
    }
}

void NotificationHandler::StopAllReceivePaths() noexcept
{
    for (auto& entry : remote_services_)
    {
        StopReceivePath(entry.second);
    }
}

}  // namespace score::someip::routing_module
