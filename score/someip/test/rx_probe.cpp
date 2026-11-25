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

#include "score/someip/routing_control_protocol/routing_control_protocol.h"
#include "score/someip/routing_module/lola_mapping/lola_mapping.h"

#include "score/filesystem/path.h"
#include "score/message_passing/client_factory.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/com/types.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{
namespace protocol = score::someip::routing_control_protocol;
namespace com = score::mw::com;
using score::someip::routing_module::EventSample;
using namespace std::chrono_literals;

constexpr std::uint16_t kService{0x1234U};
constexpr std::uint16_t kInstance{1U};
constexpr std::uint16_t kEvent{0x8001U};
constexpr std::uint16_t kLocalPort{32000U};
constexpr std::uint16_t kSenderPort{32001U};
constexpr std::uint16_t kOtherPort{32002U};

void Require(const bool condition, const char* const message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}

void SendCommand(score::message_passing::IClientConnection& connection, const protocol::Message& command)
{
    std::array<std::uint8_t, protocol::kMaxMessageSize> buffer{};
    const auto size = protocol::Encode(command, buffer);
    Require(size.has_value(), "encoding routing command failed");
    Require(connection.Send(score::cpp::span<const std::uint8_t>{buffer.data(), size.value()}).has_value(),
            "sending routing command failed");
}

class Subscriber
{
  public:
    ~Subscriber()
    {
        if (event_ != nullptr)
        {
            event_->Unsubscribe();
        }
    }

    void Connect()
    {
        const auto specifier = com::InstanceSpecifier::Create("someip/1234/0001");
        Require(specifier.has_value(), "invalid instance specifier");
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (std::chrono::steady_clock::now() < deadline)
        {
            auto handles = com::GenericProxy::FindService(specifier.value());
            Require(handles.has_value(), "finding LoLa service failed");
            if (!handles.value().empty())
            {
                auto proxy = com::GenericProxy::Create(handles.value().front());
                Require(proxy.has_value(), "creating LoLa proxy failed");
                proxy_.emplace(std::move(proxy).value());
                auto found = proxy_->GetEvents().find("event_8001");
                Require(found != proxy_->GetEvents().end(), "LoLa event missing");
                event_ = &found->second;
                Require(event_->Subscribe(4U).has_value(), "subscribing to LoLa event failed");
                return;
            }
            std::this_thread::sleep_for(10ms);
        }
        throw std::runtime_error{"LoLa service not offered"};
    }

    std::vector<EventSample> Receive()
    {
        std::vector<EventSample> samples{};
        const auto result = event_->GetNewSamples(
            [&samples](com::SamplePtr<void> sample) noexcept {
                samples.push_back(*static_cast<const EventSample*>(sample.Get()));
            },
            4U);
        Require(result.has_value(), "receiving LoLa sample failed");
        return samples;
    }

    bool HasSamplesAfterOfferStops()
    {
        bool received{false};
        static_cast<void>(event_->GetNewSamples(
            [&received](com::SamplePtr<void>) noexcept {
                received = true;
            },
            1U));
        return received;
    }

    void Disconnect()
    {
        if (event_ != nullptr)
        {
            event_->Unsubscribe();
            event_ = nullptr;
        }
        proxy_.reset();
    }

  private:
    std::optional<com::GenericProxy> proxy_{};
    com::GenericProxyEvent* event_{nullptr};
};

std::vector<std::uint8_t> Notification(const std::uint16_t session, const std::vector<std::uint8_t>& payload)
{
    const auto length = static_cast<std::uint32_t>(payload.size() + 8U);
    std::vector<std::uint8_t> bytes{0x12U,
                                    0x34U,
                                    0x80U,
                                    0x01U,
                                    static_cast<std::uint8_t>(length >> 24U),
                                    static_cast<std::uint8_t>(length >> 16U),
                                    static_cast<std::uint8_t>(length >> 8U),
                                    static_cast<std::uint8_t>(length),
                                    0U,
                                    0U,
                                    static_cast<std::uint8_t>(session >> 8U),
                                    static_cast<std::uint8_t>(session),
                                    1U,
                                    3U,
                                    2U,
                                    0U};
    bytes.insert(bytes.end(), payload.cbegin(), payload.cend());
    return bytes;
}

void CheckSample(const EventSample& sample, const std::uint16_t session, const std::vector<std::uint8_t>& payload)
{
    Require(sample.size == payload.size(), "LoLa payload size mismatch");
    Require((sample.session == session) && (sample.interface_version == 3U) && (sample.message_type == 2U),
            "LoLa metadata mismatch");
    Require(std::equal(payload.cbegin(), payload.cend(), sample.payload.cbegin()), "LoLa payload mismatch");
}

std::vector<EventSample> WaitForSamples(Subscriber& subscriber,
                                        const std::size_t count,
                                        const std::chrono::milliseconds timeout)
{
    std::vector<EventSample> samples{};
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do
    {
        auto next = subscriber.Receive();
        samples.insert(samples.end(), next.cbegin(), next.cend());
        if (samples.size() >= count)
        {
            break;
        }
        std::this_thread::sleep_for(10ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return samples;
}

void SendDatagram(const int socket, const std::uint16_t port, const std::vector<std::uint8_t>& bytes)
{
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons(port);
    Require(::sendto(socket,
                     bytes.data(),
                     bytes.size(),
                     0,
                     reinterpret_cast<const sockaddr*>(&destination),
                     sizeof(destination)) == static_cast<ssize_t>(bytes.size()),
            "sending UDP notification failed");
}

int OpenSender(const std::uint16_t port)
{
    const int socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    Require(socket >= 0, "creating UDP socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    Require(::bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0,
            "binding UDP socket failed");
    return socket;
}

void WaitForOffer(const std::string_view instance, const bool offered)
{
    const auto specifier = com::InstanceSpecifier::Create(std::string{instance});
    Require(specifier.has_value(), "invalid instance specifier");
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto handles = com::GenericProxy::FindService(specifier.value());
        Require(handles.has_value(), "finding LoLa service failed");
        if ((!handles.value().empty()) == offered)
        {
            return;
        }
        std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error{"LoLa offer did not reach expected state"};
}

void Run(const std::string_view scenario)
{
    com::runtime::InitializeRuntime(
        com::runtime::RuntimeConfiguration{score::filesystem::Path{"/opt/someip_routing/etc/mw_com_config.json"}});
    score::message_passing::ClientFactory factory{};
    const score::message_passing::ServiceProtocolConfig config{
        protocol::kServiceIdentifier, protocol::kMaxMessageSize, 0U, 0U};
    auto connection =
        factory.Create(config, score::message_passing::IClientFactory::ClientConfig{0U, 8U, false, false, false});
    Require(connection != nullptr, "creating routing client failed");
    connection->Start({}, {});
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while ((connection->GetState() != score::message_passing::IClientConnection::State::kReady) &&
           (std::chrono::steady_clock::now() < deadline))
    {
        std::this_thread::sleep_for(10ms);
    }
    Require(connection->GetState() == score::message_passing::IClientConnection::State::kReady,
            "routing client not ready");

    protocol::Message service{};
    service.opcode = protocol::Opcode::kAddRemoteService;
    service.service = kService;
    service.instance = kInstance;
    service.local_port = kLocalPort;
    service.remote_port = kSenderPort;
    service.remote_ip = {127U, 0U, 0U, 1U};
    service.events.push_back(kEvent);
    SendCommand(*connection, service);
    Subscriber subscriber{};
    subscriber.Connect();
    protocol::Message subscription{};
    subscription.opcode = protocol::Opcode::kAddLocalSubscription;
    subscription.service = kService;
    subscription.instance = kInstance;
    subscription.event = kEvent;
    SendCommand(*connection, subscription);

    const int socket = OpenSender(kSenderPort);
    const std::vector<std::uint8_t> payload{0xAAU, 0xBBU, 0xCCU};
    const auto datagram = Notification(0x42U, payload);
    bool received{false};
    const auto receive_deadline = std::chrono::steady_clock::now() + 5s;
    while ((!received) && (std::chrono::steady_clock::now() < receive_deadline))
    {
        SendDatagram(socket, kLocalPort, datagram);
        const auto samples = WaitForSamples(subscriber, 1U, 100ms);
        if (!samples.empty())
        {
            CheckSample(samples.front(), 0x42U, payload);
            received = true;
        }
    }
    Require(received, "notification did not reach LoLa with expected metadata and payload");
    static_cast<void>(WaitForSamples(subscriber, 4U, 200ms));

    if ((scenario == "wrong-event") || (scenario == "wrong-service") || (scenario == "wrong-type") ||
        (scenario == "wrong-sender") || (scenario == "wrong-port") || (scenario == "malformed") ||
        (scenario == "oversized"))
    {
        auto rejected = datagram;
        int sender = socket;
        std::uint16_t port = kLocalPort;
        if (scenario == "wrong-event")
        {
            rejected.at(3U) = 2U;
        }
        else if (scenario == "wrong-service")
        {
            rejected.at(1U) = 0x35U;
        }
        else if (scenario == "wrong-type")
        {
            rejected.at(14U) = 0x80U;
        }
        else if (scenario == "wrong-sender")
        {
            sender = OpenSender(kOtherPort);
        }
        else if (scenario == "wrong-port")
        {
            auto other_service = service;
            other_service.instance = 2U;
            other_service.local_port = kOtherPort;
            SendCommand(*connection, other_service);
            WaitForOffer("someip/1234/0002", true);
            port = kOtherPort;
        }
        else if (scenario == "malformed")
        {
            rejected.at(7U) = 7U;
        }
        else if (scenario == "oversized")
        {
            rejected = Notification(0x43U, std::vector<std::uint8_t>(1401U, 0xAAU));
        }
        SendDatagram(sender, port, rejected);
        Require(WaitForSamples(subscriber, 1U, 300ms).empty(), "invalid notification was forwarded");
        if (sender != socket)
        {
            ::close(sender);
        }
        const auto control = Notification(0x44U, payload);
        SendDatagram(socket, kLocalPort, control);
        const auto samples = WaitForSamples(subscriber, 1U, 2s);
        Require(samples.size() == 1U, "valid control notification missing");
        CheckSample(samples.front(), 0x44U, payload);
    }
    else if (scenario == "concatenated")
    {
        const std::vector<std::uint8_t> empty{};
        const std::vector<std::uint8_t> maximum(1400U, 0xABU);
        auto combined = Notification(1U, empty);
        const auto second = Notification(2U, maximum);
        const auto third = Notification(3U, payload);
        combined.insert(combined.end(), second.cbegin(), second.cend());
        combined.insert(combined.end(), third.cbegin(), third.cend());
        SendDatagram(socket, kLocalPort, combined);
        const auto samples = WaitForSamples(subscriber, 3U, 2s);
        Require(samples.size() == 3U, "concatenated notifications missing");
        CheckSample(samples.at(0U), 1U, empty);
        CheckSample(samples.at(1U), 2U, maximum);
        CheckSample(samples.at(2U), 3U, payload);
    }
    else if (scenario == "remove-subscription")
    {
        subscription.opcode = protocol::Opcode::kRemoveLocalSubscription;
        SendCommand(*connection, subscription);
        std::this_thread::sleep_for(200ms);
        SendDatagram(socket, kLocalPort, datagram);
        Require(WaitForSamples(subscriber, 1U, 400ms).empty(), "notification forwarded after subscription removal");
        subscription.opcode = protocol::Opcode::kAddLocalSubscription;
        SendCommand(*connection, subscription);
        const auto control = Notification(0x45U, payload);
        bool restored{false};
        const auto restore_deadline = std::chrono::steady_clock::now() + 5s;
        while ((!restored) && (std::chrono::steady_clock::now() < restore_deadline))
        {
            SendDatagram(socket, kLocalPort, control);
            const auto samples = WaitForSamples(subscriber, 1U, 100ms);
            if (!samples.empty())
            {
                CheckSample(samples.front(), 0x45U, payload);
                restored = true;
            }
        }
        Require(restored, "delivery did not resume after restoring the subscription");
    }
    else if ((scenario == "remove-service") || (scenario == "disconnect"))
    {
        if (scenario == "remove-service")
        {
            protocol::Message remove{};
            remove.opcode = protocol::Opcode::kRemoveRemoteService;
            remove.service = kService;
            remove.instance = kInstance;
            SendCommand(*connection, remove);
        }
        else
        {
            connection->Stop();
        }
        WaitForOffer("someip/1234/0001", false);
        SendDatagram(socket, kLocalPort, datagram);
        const auto quiet_deadline = std::chrono::steady_clock::now() + 300ms;
        while (std::chrono::steady_clock::now() < quiet_deadline)
        {
            Require(!subscriber.HasSamplesAfterOfferStops(), "notification forwarded after the service stopped");
            std::this_thread::sleep_for(10ms);
        }
        WaitForOffer("someip/1234/0001", false);
    }
    ::close(socket);
    subscriber.Disconnect();
    if (scenario != "disconnect")
    {
        connection->Stop();
    }
    WaitForOffer("someip/1234/0001", false);
}
}  // namespace

int main(const int argc, char* argv[])
{
    try
    {
        const std::string_view scenario = (argc > 1) ? argv[1] : "basic";
        Require((scenario == "basic") || (scenario == "wrong-event") || (scenario == "wrong-service") ||
                    (scenario == "wrong-type") || (scenario == "wrong-sender") || (scenario == "wrong-port") ||
                    (scenario == "malformed") || (scenario == "oversized") || (scenario == "concatenated") ||
                    (scenario == "remove-subscription") || (scenario == "remove-service") || (scenario == "disconnect"),
                "unknown RX scenario");
        Run(scenario);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
