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
#ifndef SCORE_SOMEIP_SD_CLIENT_SD_CLIENT_H
#define SCORE_SOMEIP_SD_CLIENT_SD_CLIENT_H

#include "score/someip/sd_client/sd_protocol.h"

#include "score/message_passing/i_client_factory.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>

namespace score::mw::com::impl::someip
{
class ServiceDiscoveryClient;
}

namespace score::someip_sd
{

struct ServiceDetails
{
    std::uint16_t service;
    std::uint16_t instance;
    std::uint8_t major;
    std::uint32_t minor;
};

/// Application side of the opcode protocol towards the SD daemon.
class SdClient final
{
  public:
    using AvailabilityHandler = std::function<void(std::uint16_t service, std::uint16_t instance, bool available)>;
    using SubscriptionStateHandler =
        std::function<void(std::uint16_t service, std::uint16_t instance, std::uint16_t eventgroup, bool acknowledged)>;
    /// Handlers are called on connection callback threads. The owner and handlers must outlive all callbacks.
    SdClient(score::message_passing::IClientFactory& client_factory,
             AvailabilityHandler availability_handler,
             SubscriptionStateHandler subscription_state_handler);
    ~SdClient();

    SdClient(const SdClient&) = delete;
    SdClient& operator=(const SdClient&) = delete;
    SdClient(SdClient&&) = delete;
    SdClient& operator=(SdClient&&) = delete;

    /// Bounds connection and registration waits. Failed attempts stop and release the connection before returning.
    /// A live registered connection returns its existing client id.
    std::optional<std::uint16_t> Connect(std::chrono::milliseconds timeout);

    bool RequestService(std::uint16_t service, std::uint16_t instance, std::uint8_t major, std::uint32_t minor);
    bool RequestService(const ServiceDetails& details);
    bool ReleaseService(std::uint16_t service, std::uint16_t instance);
    bool Subscribe(std::uint16_t service, std::uint16_t instance, std::uint16_t eventgroup, std::uint8_t major);
    bool Unsubscribe(std::uint16_t service, std::uint16_t instance, std::uint16_t eventgroup);
    /// The service instance must be configured with is-provider true in the SD daemon's configuration.
    bool OfferService(std::uint16_t service, std::uint16_t instance, std::uint8_t major, std::uint32_t minor);
    bool StopOfferService(std::uint16_t service, std::uint16_t instance);

  private:
    using ConnectionLostHandler = std::function<void()>;

    SdClient(score::message_passing::IClientFactory& client_factory,
             AvailabilityHandler availability_handler,
             SubscriptionStateHandler subscription_state_handler,
             ConnectionLostHandler connection_lost_handler);

    friend class score::mw::com::impl::someip::ServiceDiscoveryClient;

    bool Send(const protocol::Message& message);
    void OnNotify(score::cpp::span<const std::uint8_t> data);
    void StopAndResetConnection();

    AvailabilityHandler availability_handler_;
    SubscriptionStateHandler subscription_state_handler_;
    ConnectionLostHandler connection_lost_handler_;

    score::message_passing::IClientFactory& client_factory_;
    score::cpp::pmr::unique_ptr<score::message_passing::IClientConnection> connection_;

    std::mutex lifecycle_mutex_;
    std::mutex connection_mutex_;
    std::mutex state_mutex_;
    std::condition_variable state_condition_;
    bool intentional_shutdown_{false};
    bool session_was_ready_{false};
    bool session_loss_pending_{false};
    score::message_passing::IClientConnection::State state_{
        score::message_passing::IClientConnection::State::kStarting};
    std::optional<std::uint16_t> client_id_;
};

}  // namespace score::someip_sd

#endif  // SCORE_SOMEIP_SD_CLIENT_SD_CLIENT_H
