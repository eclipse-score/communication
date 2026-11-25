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

#include "score/someip/routing_module/lola_mapping/lola_mapping.h"
#include "score/someip/routing_module/routing_bridge/routing_bridge.h"
#include "score/someip/routing_module/udp_transport/linux/transport.h"

#include "score/filesystem/path.h"
#include "score/message_passing/mock/server_connection_mock.h"
#include "score/message_passing/mock/server_factory_mock.h"
#include "score/message_passing/mock/server_mock.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
score::someip::routing_module::RoutingBridge* active_bridge{nullptr};
}

extern "C" void someip_routing_poll(void)
{
    if (active_bridge != nullptr)
    {
        active_bridge->Poll();
    }
}

extern "C" void someip_routing_datagram(const unsigned short local_port,
                                        const unsigned char sender_ip[4],
                                        const unsigned short sender_port,
                                        const void* const packet,
                                        const unsigned int size)
{
    score::someip::routing_module::Endpoint sender{{}, sender_port};
    std::copy_n(sender_ip, sender.address.size(), sender.address.begin());
    active_bridge->OnUdpDatagram(local_port, sender, packet, size);
}

namespace score::someip::routing_module
{
namespace
{
using score::message_passing::ClientIdentity;
using score::message_passing::ConnectCallback;
using score::message_passing::DisconnectCallback;
using score::message_passing::MessageCallback;
using score::message_passing::ServerConnectionMock;
using score::message_passing::ServerFactoryMock;
using score::message_passing::ServerMock;
using score::message_passing::UserData;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr std::uint16_t kInstance{1U};
constexpr std::uint16_t kEvent{0x8001U};

std::uint16_t Service()
{
    static const auto service = static_cast<std::uint16_t>(0x4000U | (static_cast<unsigned int>(::getpid()) & 0x3FFFU));
    return service;
}

std::string WriteLolaConfig()
{
    const char* const test_tmpdir = std::getenv("TEST_TMPDIR");
    const std::filesystem::path directory =
        (test_tmpdir != nullptr) ? std::filesystem::path{test_tmpdir} : std::filesystem::temp_directory_path();
    const auto path = directory / ("linux_bridge_test_" + std::to_string(::getpid()) + ".json");
    std::ofstream{path}
        << R"({"serviceTypes": [{"serviceTypeName": "/someip/service", "version": {"major": 1, "minor": 0}, )"
           R"("bindings": [{"binding": "SHM", "serviceId": )"
        << Service() << R"(, "events": [{"eventName": ")" << LolaEventName(kEvent) << R"(", "eventId": 1}]}]}], )"
        << R"("serviceInstances": [{"instanceSpecifier": ")" << LolaInstanceSpecifier(Service(), kInstance)
        << R"(", "serviceTypeName": "/someip/service", "version": {"major": 1, "minor": 0}, "instances": [{)"
           R"("instanceId": 1, "asil-level": "QM", "binding": "SHM", "events": [{"eventName": ")"
        << LolaEventName(kEvent) << R"(", "numberOfSampleSlots": 4, "maxSubscribers": 1}]}]}]})";
    return path.string();
}

class Socket
{
  public:
    Socket() : descriptor_{::socket(AF_INET, SOCK_DGRAM, 0)} {}
    ~Socket()
    {
        if (descriptor_ >= 0)
        {
            ::close(descriptor_);
        }
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    int Get() const noexcept
    {
        return descriptor_;
    }
    std::uint16_t BindLoopback()
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(descriptor_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
        {
            return 0U;
        }
        socklen_t length = sizeof(address);
        return (::getsockname(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) == 0)
                   ? ntohs(address.sin_port)
                   : 0U;
    }

  private:
    int descriptor_;
};

class LinuxBridgeTest : public ::testing::Test
{
  protected:
    static void SetUpTestSuite()
    {
        score::mw::com::runtime::InitializeRuntime(
            score::mw::com::runtime::RuntimeConfiguration{score::filesystem::Path{WriteLolaConfig()}});
    }

    void SetUp() override
    {
        ASSERT_EQ(someip_linux_transport_init(), 0);
        ON_CALL(*server_, StartListening(_, _, _, _))
            .WillByDefault([this](ConnectCallback connect, DisconnectCallback, MessageCallback command, MessageCallback)
                               -> score::cpp::expected_blank<score::os::Error> {
                connect_ = std::move(connect);
                command_ = std::move(command);
                return {};
            });
        ON_CALL(factory_, Create(_, _)).WillByDefault(Return(::testing::ByMove(std::move(server_))));
        ON_CALL(controller_, GetClientIdentity()).WillByDefault(ReturnRef(identity_));
        ON_CALL(controller_, GetUserData()).WillByDefault(ReturnRef(user_data_));
        bridge_.emplace(factory_);
        active_bridge = &bridge_.value();
        ASSERT_TRUE(bridge_->Start().has_value());
        auto user_data = connect_(controller_);
        ASSERT_TRUE(user_data.has_value());
        user_data_ = std::move(user_data).value();
    }

    void TearDown() override
    {
        bridge_.reset();
        active_bridge = nullptr;
        someip_linux_transport_close();
    }

    void SendCommand(const routing_control_protocol::Message& command)
    {
        std::array<std::uint8_t, routing_control_protocol::kMaxMessageSize> buffer{};
        const auto size = routing_control_protocol::Encode(command, buffer);
        ASSERT_TRUE(size.has_value());
        ASSERT_TRUE(
            command_(controller_, score::cpp::span<const std::uint8_t>{buffer.data(), size.value()}).has_value());
    }

    void RunReceiveScenario(std::string_view scenario);

    NiceMock<ServerFactoryMock> factory_{};
    score::cpp::pmr::unique_ptr<NiceMock<ServerMock>> server_{
        score::cpp::pmr::make_unique<NiceMock<ServerMock>>(score::cpp::pmr::get_default_resource())};
    NiceMock<ServerConnectionMock> controller_{};
    ClientIdentity identity_{100, 1000U, 1000U};
    UserData user_data_{};
    ConnectCallback connect_{};
    MessageCallback command_{};
    std::optional<RoutingBridge> bridge_{};
};

void LinuxBridgeTest::RunReceiveScenario(const std::string_view scenario)
{
    Socket sender{};
    ASSERT_GE(sender.Get(), 0);
    const auto sender_port = sender.BindLoopback();
    ASSERT_NE(sender_port, 0U);
    std::uint16_t local_port{0U};
    {
        Socket reservation{};
        ASSERT_GE(reservation.Get(), 0);
        local_port = reservation.BindLoopback();
    }
    ASSERT_NE(local_port, 0U);
    routing_control_protocol::Message service{};
    service.opcode = routing_control_protocol::Opcode::kAddRemoteService;
    service.service = Service();
    service.instance = kInstance;
    service.local_port = local_port;
    service.remote_ip = {127U, 0U, 0U, 1U};
    service.remote_port = sender_port;
    service.events.push_back(kEvent);
    SendCommand(service);
    routing_control_protocol::Message subscription{};
    subscription.opcode = routing_control_protocol::Opcode::kAddLocalSubscription;
    subscription.service = Service();
    subscription.instance = kInstance;
    subscription.event = kEvent;
    SendCommand(subscription);
    ASSERT_EQ(someip_linux_transport_poll(0), 0);

    const auto specifier = score::mw::com::InstanceSpecifier::Create(LolaInstanceSpecifier(Service(), kInstance));
    ASSERT_TRUE(specifier.has_value());
    const auto handles = score::mw::com::GenericProxy::FindService(specifier.value());
    ASSERT_TRUE(handles.has_value());
    ASSERT_EQ(handles.value().size(), 1U);
    auto proxy = score::mw::com::GenericProxy::Create(handles.value().front());
    ASSERT_TRUE(proxy.has_value());
    auto event = proxy.value().GetEvents().find(LolaEventName(kEvent));
    ASSERT_NE(event, proxy.value().GetEvents().end());
    ASSERT_TRUE(event->second.Subscribe(4U).has_value());
    struct UnsubscribeOnExit
    {
        score::mw::com::GenericProxyEvent& event;
        ~UnsubscribeOnExit()
        {
            event.Unsubscribe();
        }
    } unsubscribe{event->second};
    const std::array<std::uint8_t, 19U> datagram{static_cast<std::uint8_t>(Service() >> 8U),
                                                 static_cast<std::uint8_t>(Service()),
                                                 0x80U,
                                                 0x01U,
                                                 0U,
                                                 0U,
                                                 0U,
                                                 11U,
                                                 0U,
                                                 0U,
                                                 0U,
                                                 0x42U,
                                                 1U,
                                                 3U,
                                                 2U,
                                                 0U,
                                                 0xAAU,
                                                 0xBBU,
                                                 0xCCU};
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons(local_port);
    EXPECT_EQ(::sendto(sender.Get(),
                       datagram.data(),
                       datagram.size(),
                       0,
                       reinterpret_cast<const sockaddr*>(&destination),
                       sizeof(destination)),
              static_cast<ssize_t>(datagram.size()));
    EXPECT_EQ(someip_linux_transport_poll(1000), 0);
    std::vector<EventSample> samples{};
    const auto result = event->second.GetNewSamples(
        [&samples](score::mw::com::SamplePtr<void> sample) noexcept {
            samples.push_back(*static_cast<const EventSample*>(sample.Get()));
        },
        1U);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(samples.size(), 1U);
    if (!samples.empty())
    {
        EXPECT_EQ(samples.front().size, 3U);
        EXPECT_EQ(samples.front().session, 0x42U);
        EXPECT_EQ(samples.front().interface_version, 3U);
        EXPECT_EQ(samples.front().message_type, 2U);
        EXPECT_EQ(samples.front().payload.at(0U), 0xAAU);
        EXPECT_EQ(samples.front().payload.at(1U), 0xBBU);
        EXPECT_EQ(samples.front().payload.at(2U), 0xCCU);
    }
    if (scenario == "basic")
    {
        return;
    }
    samples.clear();
    if (scenario == "concatenated")
    {
        std::vector<std::uint8_t> combined{datagram.cbegin(), datagram.cbegin() + 16};
        combined.at(7U) = 8U;
        combined.at(11U) = 1U;
        std::vector<std::uint8_t> maximum{datagram.cbegin(), datagram.cbegin() + 16};
        const auto length = static_cast<std::uint32_t>(kMaxPayloadSize + 8U);
        maximum.at(6U) = static_cast<std::uint8_t>(length >> 8U);
        maximum.at(7U) = static_cast<std::uint8_t>(length);
        maximum.at(11U) = 2U;
        maximum.insert(maximum.end(), kMaxPayloadSize, 0xABU);
        combined.insert(combined.end(), maximum.cbegin(), maximum.cend());
        combined.insert(combined.end(), datagram.cbegin(), datagram.cend());
        ASSERT_EQ(::sendto(sender.Get(),
                           combined.data(),
                           combined.size(),
                           0,
                           reinterpret_cast<const sockaddr*>(&destination),
                           sizeof(destination)),
                  static_cast<ssize_t>(combined.size()));
        ASSERT_EQ(someip_linux_transport_poll(1000), 0);
        const auto combined_result = event->second.GetNewSamples(
            [&samples](score::mw::com::SamplePtr<void> sample) noexcept {
                samples.push_back(*static_cast<const EventSample*>(sample.Get()));
            },
            4U);
        ASSERT_TRUE(combined_result.has_value());
        ASSERT_EQ(samples.size(), 3U);
        EXPECT_EQ(samples.at(0U).size, 0U);
        EXPECT_EQ(samples.at(0U).session, 1U);
        EXPECT_EQ(samples.at(1U).size, kMaxPayloadSize);
        EXPECT_EQ(samples.at(1U).session, 2U);
        EXPECT_TRUE(
            std::all_of(samples.at(1U).payload.cbegin(), samples.at(1U).payload.cend(), [](const std::uint8_t byte) {
                return byte == 0xABU;
            }));
        EXPECT_EQ(samples.at(2U).size, 3U);
        EXPECT_EQ(samples.at(2U).session, 0x42U);
        EXPECT_TRUE(std::equal(datagram.cbegin() + 16, datagram.cend(), samples.at(2U).payload.cbegin()));
        for (const auto& sample : samples)
        {
            EXPECT_EQ(sample.interface_version, 3U);
            EXPECT_EQ(sample.message_type, 2U);
        }
        return;
    }
    std::vector<std::uint8_t> rejected{datagram.cbegin(), datagram.cend()};
    Socket other_sender{};
    int sending_descriptor = sender.Get();
    if (scenario == "wrong-sender")
    {
        ASSERT_GE(other_sender.Get(), 0);
        ASSERT_NE(other_sender.BindLoopback(), 0U);
        sending_descriptor = other_sender.Get();
    }
    else if (scenario == "wrong-event")
    {
        rejected.at(3U) = 2U;
    }
    else if (scenario == "wrong-service")
    {
        rejected.at(1U) ^= 1U;
    }
    else if (scenario == "wrong-type")
    {
        rejected.at(14U) = 0x80U;
    }
    else if (scenario == "malformed")
    {
        rejected.at(7U) = 7U;
    }
    else if (scenario == "oversized")
    {
        const auto length = static_cast<std::uint32_t>(kMaxPayloadSize + 9U);
        rejected.at(6U) = static_cast<std::uint8_t>(length >> 8U);
        rejected.at(7U) = static_cast<std::uint8_t>(length);
        rejected.resize(16U + kMaxPayloadSize + 1U, 0xABU);
    }
    else if (scenario == "remove-subscription")
    {
        subscription.opcode = routing_control_protocol::Opcode::kRemoveLocalSubscription;
        SendCommand(subscription);
        ASSERT_EQ(someip_linux_transport_poll(0), 0);
    }
    EXPECT_EQ(::sendto(sending_descriptor,
                       rejected.data(),
                       rejected.size(),
                       0,
                       reinterpret_cast<const sockaddr*>(&destination),
                       sizeof(destination)),
              static_cast<ssize_t>(rejected.size()));
    EXPECT_EQ(someip_linux_transport_poll(1000), 0);
    const auto rejected_result = event->second.GetNewSamples(
        [&samples](score::mw::com::SamplePtr<void> sample) noexcept {
            samples.push_back(*static_cast<const EventSample*>(sample.Get()));
        },
        1U);
    EXPECT_TRUE(rejected_result.has_value());
    EXPECT_TRUE(samples.empty());
    if (scenario == "remove-subscription")
    {
        subscription.opcode = routing_control_protocol::Opcode::kAddLocalSubscription;
        SendCommand(subscription);
        ASSERT_EQ(someip_linux_transport_poll(0), 0);
    }
    EXPECT_EQ(::sendto(sender.Get(),
                       datagram.data(),
                       datagram.size(),
                       0,
                       reinterpret_cast<const sockaddr*>(&destination),
                       sizeof(destination)),
              static_cast<ssize_t>(datagram.size()));
    EXPECT_EQ(someip_linux_transport_poll(1000), 0);
    const auto control_result = event->second.GetNewSamples(
        [&samples](score::mw::com::SamplePtr<void> sample) noexcept {
            samples.push_back(*static_cast<const EventSample*>(sample.Get()));
        },
        1U);
    EXPECT_TRUE(control_result.has_value());
    EXPECT_EQ(samples.size(), 1U);
}

TEST_F(LinuxBridgeTest, DeliversUdpNotificationThroughTheRealRoutingPath)
{
    RecordProperty("lobster-tracing",
                   "RoutingModule.CR_RoutingModule_ReceiveRouteMatch,"
                   "RoutingModule.CR_RoutingModule_PayloadPreservation,"
                   "RoutingModule.CR_RoutingModule_ReceiveMetadata");
    RecordProperty("given", "a Linux UDP transport with a remote service and local event subscription");
    RecordProperty("when", "a matching SOME/IP notification arrives over loopback UDP");
    RecordProperty("then", "the routing bridge publishes the original payload and metadata into LoLa");
    RunReceiveScenario("basic");
}

TEST_F(LinuxBridgeTest, DoesNotForwardNotificationsFromAnotherSender)
{
    RecordProperty("lobster-tracing", "RoutingModule.CR_RoutingModule_ReceiveRouteMatch");
    RecordProperty("given", "a subscribed event with an active Linux UDP receive route");
    RecordProperty("when", "a notification arrives from a different source port");
    RecordProperty("then", "it is dropped and a later matching notification is delivered");
    RunReceiveScenario("wrong-sender");
}

TEST_F(LinuxBridgeTest, DoesNotForwardNotificationsForAnUnknownEvent)
{
    RecordProperty("lobster-tracing", "RoutingModule.CR_RoutingModule_ReceiveRouteMatch");
    RecordProperty("given", "a subscribed event with an active Linux UDP receive route");
    RecordProperty("when", "a notification for an unknown event arrives");
    RecordProperty("then", "it is dropped and a later matching notification is delivered");
    RunReceiveScenario("wrong-event");
}

TEST_F(LinuxBridgeTest, DoesNotForwardNotificationsForAnotherService)
{
    RecordProperty("lobster-tracing", "RoutingModule.CR_RoutingModule_ReceiveRouteMatch");
    RecordProperty("given", "a subscribed event with an active Linux UDP receive route");
    RecordProperty("when", "a notification for another service arrives");
    RecordProperty("then", "it is dropped and a later matching notification is delivered");
    RunReceiveScenario("wrong-service");
}

TEST_F(LinuxBridgeTest, DoesNotForwardResponses)
{
    RecordProperty("lobster-tracing", "RoutingModule.CR_RoutingModule_NotificationOnly");
    RecordProperty("given", "a subscribed event with an active Linux UDP receive route");
    RecordProperty("when", "a SOME/IP response arrives");
    RecordProperty("then", "it is dropped and a later matching notification is delivered");
    RunReceiveScenario("wrong-type");
}

TEST_F(LinuxBridgeTest, DropsMalformedNotifications)
{
    RecordProperty("lobster-tracing", "RoutingModule.CR_RoutingModule_InvalidFraming");
    RecordProperty("given", "a subscribed event with an active Linux UDP receive route");
    RecordProperty("when", "a notification with an invalid SOME/IP length arrives");
    RecordProperty("then", "it is dropped and a later matching notification is delivered");
    RunReceiveScenario("malformed");
}

TEST_F(LinuxBridgeTest, DropsOversizedNotifications)
{
    RecordProperty("lobster-tracing", "RoutingModule.CR_RoutingModule_OversizedPayload");
    RecordProperty("given", "a subscribed event with an active Linux UDP receive route");
    RecordProperty("when", "a notification with a 1401-byte payload arrives");
    RecordProperty("then", "it is dropped and a later matching notification is delivered");
    RunReceiveScenario("oversized");
}

TEST_F(LinuxBridgeTest, DeliversConcatenatedEmptyAndMaximumSizeNotifications)
{
    RecordProperty("lobster-tracing",
                   "RoutingModule.CR_RoutingModule_ConcatenatedMessages,"
                   "RoutingModule.CR_RoutingModule_EmptyPayload,"
                   "RoutingModule.CR_RoutingModule_PayloadCapacity");
    RecordProperty("given", "a subscribed event with an active Linux UDP receive route");
    RecordProperty("when", "one datagram carries empty, maximum-size and short notifications");
    RecordProperty("then", "all three notifications are delivered in order with their original payloads");
    RunReceiveScenario("concatenated");
}

TEST_F(LinuxBridgeTest, StopsForwardingWhileSubscriptionIsRemoved)
{
    RecordProperty("lobster-tracing", "RoutingModule.CR_RoutingModule_LocalSubscriptions");
    RecordProperty("given", "a subscribed event with an active Linux UDP receive route");
    RecordProperty("when", "the subscription is removed and later restored");
    RecordProperty("then", "notifications are dropped while unsubscribed and delivered after restoration");
    RunReceiveScenario("remove-subscription");
}
}  // namespace
}  // namespace score::someip::routing_module
