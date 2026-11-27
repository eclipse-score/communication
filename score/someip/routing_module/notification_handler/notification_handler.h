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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_NOTIFICATION_HANDLER_NOTIFICATION_HANDLER_H
#define SCORE_SOMEIP_ROUTING_MODULE_NOTIFICATION_HANDLER_NOTIFICATION_HANDLER_H

#include "score/someip/routing_module/notification_handler/someip_datagram_parser.h"
#include "score/someip/routing_module/routing_manager/i_routing_command_handler.h"
#include "score/someip/routing_module/udp_transport/log.h"
#include "score/someip/routing_module/udp_transport/udp_packet_reader.h"

#include "score/mw/com/types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace score::someip::routing_module
{

/// Moves SOME/IP notifications between UDP and LoLa: offers each remote service as LoLa service instance and
/// tracks the local subscriptions of its events. Called on the data path thread only.
class NotificationHandler final : public IRoutingCommandHandler
{
  public:
    NotificationHandler() noexcept;
    ~NotificationHandler() noexcept override;
    NotificationHandler(const NotificationHandler&) = delete;
    NotificationHandler(NotificationHandler&&) = delete;
    NotificationHandler& operator=(const NotificationHandler&) = delete;
    NotificationHandler& operator=(NotificationHandler&&) = delete;

    CommandResult AddRemoteService(const routing_control_protocol::Message& command) noexcept override;
    CommandResult RemoveRemoteService(ServiceInstance service_instance) noexcept override;
    CommandResult AddLocalSubscription(ServiceInstance service_instance, std::uint16_t event) noexcept override;
    CommandResult RemoveLocalSubscription(ServiceInstance service_instance, std::uint16_t event) noexcept override;
    CommandResult AddLocalService(const routing_control_protocol::Message& command) noexcept override;
    CommandResult RemoveLocalService(ServiceInstance service_instance) noexcept override;
    CommandResult AddRemoteSubscription(ServiceInstance service_instance,
                                        std::uint16_t event,
                                        Endpoint subscriber) noexcept override;
    CommandResult RemoveRemoteSubscription(ServiceInstance service_instance,
                                           std::uint16_t event,
                                           Endpoint subscriber) noexcept override;
    void RemoveAll() noexcept override;

    /// Forwards the locally subscribed notifications of a UDP datagram received on udp_local_port into LoLa.
    void OnUdpDatagram(std::uint16_t udp_local_port,
                       const Endpoint& udp_sender,
                       const UdpPacketReader& datagram) noexcept;

  private:
    enum class DropReason : std::size_t
    {
        kNonNotification,
        kMalformed,
        kNoReceivePath,
        kUnknownEvent,
        kNotSubscribed,
        kOversizedPayload,
        kNoSample,
        kCopyFailed,
        kSendFailed,
        kCount,
    };

    /// A SOME/IP event of a remote service and the LoLa event its notifications are forwarded to.
    struct RemoteEvent
    {
        score::mw::com::GenericSkeletonEvent* lola_event;
        bool local_subscription;
    };

    /// Receive path of a remote service; it is active while the LoLa skeleton is offered.
    struct RemoteService
    {
        std::uint16_t udp_local_port;
        Endpoint udp_remote_endpoint;
        std::unique_ptr<score::mw::com::GenericSkeleton> lola_skeleton;
        bool lola_offered;
        std::map<std::uint16_t, RemoteEvent> someip_events;
    };

    using SomeipServiceInstance = std::pair<std::uint16_t, std::uint16_t>;
    using RemoteServices = std::map<SomeipServiceInstance, RemoteService>;

    static std::optional<RemoteService> CreateLolaSkeleton(const routing_control_protocol::Message& command) noexcept;
    bool UdpRouteUsedByOtherInstance(const routing_control_protocol::Message& command) noexcept;
    RemoteServices::iterator FindReceivePath(std::uint16_t someip_service,
                                             std::uint16_t udp_local_port,
                                             const Endpoint& udp_sender) noexcept;
    RemoteEvent* FindLolaOfferedEvent(ServiceInstance service_instance, std::uint16_t event) noexcept;
    void ForwardToLola(std::uint16_t udp_local_port,
                       const Endpoint& udp_sender,
                       const SomeipMessageFrame& notification,
                       const UdpPacketReader& datagram) noexcept;
    static void StopReceivePath(RemoteService& remote_service) noexcept;
    void StopAllReceivePaths() noexcept;
    std::optional<std::uint64_t> ShouldLogDrop(DropReason reason) noexcept;
    void LogDroppedNotification(DropReason reason,
                                std::string_view description,
                                const SomeipMessageHeader& header,
                                std::uint16_t someip_instance) noexcept;

    RemoteServices remote_services_;
    std::array<someip_log_limiter, static_cast<std::size_t>(DropReason::kCount)> drop_log_limiters_{};
};

}  // namespace score::someip::routing_module

#endif  // SCORE_SOMEIP_ROUTING_MODULE_NOTIFICATION_HANDLER_NOTIFICATION_HANDLER_H
