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

#include "score/someip/routing_module/lola_mapping/lola_mapping.h"
#include "score/someip/routing_module/udp_transport/udp_transport.h"

#include "score/filesystem/path.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"
#include "score/mw/log/logging.h"
#include "score/mw/log/recorder_mock.h"

#include <gtest/gtest.h>

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct FakeUdpTransport
{
    std::vector<unsigned short> opened_ports;
    int open_error{0};
    std::size_t packet_copies{0U};
    std::size_t fail_packet_copy{0U};
};

FakeUdpTransport& UdpTransport()
{
    static FakeUdpTransport transport{};
    return transport;
}

}  // namespace

extern "C" void someip_routing_log(someip_log_severity, const char*) {}

/// Test doubles of the platform transport; a received packet is a std::vector<std::uint8_t> here.
extern "C" int someip_transport_open_udp_socket(const unsigned short port)
{
    if (UdpTransport().open_error != 0)
    {
        return UdpTransport().open_error;
    }
    UdpTransport().opened_ports.push_back(port);
    return 0;
}

extern "C" int someip_transport_copy_packet(const void* const packet,
                                            const unsigned int offset,
                                            const unsigned int size,
                                            void* const destination)
{
    ++UdpTransport().packet_copies;
    if (UdpTransport().packet_copies == UdpTransport().fail_packet_copy)
    {
        return EIO;
    }
    const auto& bytes = *static_cast<const std::vector<std::uint8_t>*>(packet);
    std::copy_n(bytes.cbegin() + static_cast<std::ptrdiff_t>(offset), size, static_cast<std::uint8_t*>(destination));
    return 0;
}

namespace score::someip::routing_module
{
namespace
{

constexpr std::uint16_t kConfiguredInstance{0x0001U};
constexpr std::uint16_t kOtherConfiguredInstance{0x0002U};
constexpr std::uint16_t kUnconfiguredInstance{0x0003U};
constexpr std::uint16_t kEvent{0x8001U};
constexpr std::uint16_t kOtherEvent{0x8002U};
constexpr std::uint16_t kUnknownEvent{0x8003U};
constexpr std::uint16_t kUdpLocalPort{30501U};
constexpr std::uint16_t kUdpRemotePort{30502U};
constexpr std::size_t kLolaSampleSlots{4U};
constexpr std::string_view kServiceTypeName{"/someip/service"};
constexpr std::string_view kOtherServiceTypeName{"/someip/other_service"};
const Endpoint kUdpSender{routing_control_protocol::Ipv4Address{10U, 0U, 3U, 2U}, kUdpRemotePort};

/// LoLa's shared memory and discovery paths are global, so concurrent runs of this test use distinct services.
std::uint16_t Service()
{
    static const auto service = static_cast<std::uint16_t>(0x4000U | (static_cast<unsigned int>(::getpid()) & 0x3FFFU));
    return service;
}

std::uint16_t OtherService()
{
    return static_cast<std::uint16_t>(Service() + 1U);
}

std::string LolaServiceTypeJson(const std::uint16_t service, const std::string_view service_type_name)
{
    return R"({"serviceTypeName": ")" + std::string{service_type_name} +
           R"(", "version": {"major": 1, "minor": 0}, "bindings": [{"binding": "SHM", "serviceId": )" +
           std::to_string(service) + R"(, "events": [{"eventName": ")" + LolaEventName(kEvent) +
           R"(", "eventId": 1}, {"eventName": ")" + LolaEventName(kOtherEvent) + R"(", "eventId": 2}]}]})";
}

std::string LolaServiceInstanceJson(const std::uint16_t service,
                                    const std::string_view service_type_name,
                                    const std::uint16_t instance)
{
    return R"({"instanceSpecifier": ")" + LolaInstanceSpecifier(service, instance) + R"(", "serviceTypeName": ")" +
           std::string{service_type_name} + R"(", "version": {"major": 1, "minor": 0}, "instances": [{"instanceId": )" +
           std::to_string(instance) +
           R"(, "asil-level": "QM", "binding": "SHM", "events": [)"
           R"({"eventName": ")" +
           LolaEventName(kEvent) +
           R"(", "numberOfSampleSlots": 4, "maxSubscribers": 2}, )"
           R"({"eventName": ")" +
           LolaEventName(kOtherEvent) + R"(", "numberOfSampleSlots": 4, "maxSubscribers": 2}]}]})";
}

std::string WriteLolaConfig()
{
    const char* const test_tmpdir = std::getenv("TEST_TMPDIR");
    const std::filesystem::path directory =
        (test_tmpdir != nullptr) ? std::filesystem::path{test_tmpdir} : std::filesystem::temp_directory_path();
    const std::filesystem::path path =
        directory / ("notification_handler_test_" + std::to_string(::getpid()) + ".json");
    std::ofstream{path} << R"({"serviceTypes": [)" << LolaServiceTypeJson(Service(), kServiceTypeName) << ", "
                        << LolaServiceTypeJson(OtherService(), kOtherServiceTypeName) << R"(], "serviceInstances": [)"
                        << LolaServiceInstanceJson(Service(), kServiceTypeName, kConfiguredInstance) << ", "
                        << LolaServiceInstanceJson(Service(), kServiceTypeName, kOtherConfiguredInstance) << ", "
                        << LolaServiceInstanceJson(OtherService(), kOtherServiceTypeName, kConfiguredInstance) << "]}";
    return path.string();
}

routing_control_protocol::Message RemoteService(const std::uint16_t instance,
                                                const std::uint16_t udp_local_port = kUdpLocalPort,
                                                const std::uint16_t udp_remote_port = kUdpRemotePort,
                                                const std::uint16_t service = Service())
{
    routing_control_protocol::Message message{};
    message.opcode = routing_control_protocol::Opcode::kAddRemoteService;
    message.service = service;
    message.instance = instance;
    message.local_port = udp_local_port;
    message.remote_port = udp_remote_port;
    message.remote_ip = routing_control_protocol::Ipv4Address{10U, 0U, 3U, 2U};
    message.events.push_back(kEvent);
    message.events.push_back(kOtherEvent);
    return message;
}

ServiceInstance Instance(const std::uint16_t instance)
{
    return ServiceInstance{Service(), instance};
}

struct Notification
{
    std::uint16_t service;
    std::uint16_t event;
    std::vector<std::uint8_t> payload;
    std::uint16_t session{0x1234U};
    std::uint8_t interface_version{0x03U};
    std::uint8_t message_type{0x02U};
};

/// Appends a SOME/IP message as received from the network to a UDP datagram.
void Append(std::vector<std::uint8_t>& datagram, const Notification& notification)
{
    const auto length = static_cast<std::uint32_t>(notification.payload.size() + 8U);
    const std::vector<std::uint8_t> header{static_cast<std::uint8_t>(notification.service >> 8U),
                                           static_cast<std::uint8_t>(notification.service),
                                           static_cast<std::uint8_t>(notification.event >> 8U),
                                           static_cast<std::uint8_t>(notification.event),
                                           static_cast<std::uint8_t>(length >> 24U),
                                           static_cast<std::uint8_t>(length >> 16U),
                                           static_cast<std::uint8_t>(length >> 8U),
                                           static_cast<std::uint8_t>(length),
                                           0x00U,
                                           0x00U,
                                           static_cast<std::uint8_t>(notification.session >> 8U),
                                           static_cast<std::uint8_t>(notification.session),
                                           0x01U,
                                           notification.interface_version,
                                           notification.message_type,
                                           0x00U};
    datagram.insert(datagram.end(), header.cbegin(), header.cend());
    datagram.insert(datagram.end(), notification.payload.cbegin(), notification.payload.cend());
}

std::vector<std::uint8_t> Datagram(const Notification& notification)
{
    std::vector<std::uint8_t> datagram{};
    Append(datagram, notification);
    return datagram;
}

std::vector<std::uint8_t> Payload(const std::size_t size, const std::uint8_t first = 0x10U)
{
    std::vector<std::uint8_t> payload(size);
    for (std::size_t index = 0U; index < size; ++index)
    {
        payload.at(index) = static_cast<std::uint8_t>(first + index);
    }
    return payload;
}

UdpPacketReader Reader(const std::vector<std::uint8_t>& datagram)
{
    return UdpPacketReader{&datagram, static_cast<unsigned int>(datagram.size())};
}

/// LoLa consumer of one event of a remote service instance; keeps received samples until they are released.
class LolaSubscriber
{
  public:
    LolaSubscriber(const std::uint16_t instance, const std::uint16_t event)
    {
        auto specifier = score::mw::com::InstanceSpecifier::Create(LolaInstanceSpecifier(Service(), instance));
        EXPECT_TRUE(specifier.has_value());
        auto handles = score::mw::com::GenericProxy::FindService(specifier.value());
        EXPECT_TRUE(handles.has_value() && (handles.value().size() == 1U));
        auto proxy = score::mw::com::GenericProxy::Create(handles.value().front());
        EXPECT_TRUE(proxy.has_value());
        lola_proxy_.emplace(std::move(proxy).value());
        lola_event_ = &lola_proxy_->GetEvents().find(LolaEventName(event))->second;
        EXPECT_TRUE(lola_event_->Subscribe(kLolaSampleSlots).has_value());
    }

    // GenericProxy does not unsubscribe its events on destruction; LoLa aborts if they are still subscribed.
    ~LolaSubscriber()
    {
        held_samples_.clear();
        lola_event_->Unsubscribe();
    }

    LolaSubscriber(const LolaSubscriber&) = delete;
    LolaSubscriber& operator=(const LolaSubscriber&) = delete;
    LolaSubscriber(LolaSubscriber&&) = delete;
    LolaSubscriber& operator=(LolaSubscriber&&) = delete;

    /// Takes the samples received since the previous call, oldest first.
    std::vector<EventSample> Receive()
    {
        std::vector<EventSample> received{};
        static_cast<void>(lola_event_->GetNewSamples(
            [this, &received](score::mw::com::SamplePtr<void> sample) noexcept {
                received.push_back(*static_cast<const EventSample*>(sample.Get()));
                held_samples_.push_back(std::move(sample));
            },
            kLolaSampleSlots));
        return received;
    }

    void ReleaseSamples()
    {
        held_samples_.clear();
    }

  private:
    std::optional<score::mw::com::GenericProxy> lola_proxy_{};
    score::mw::com::GenericProxyEvent* lola_event_{nullptr};
    std::vector<score::mw::com::SamplePtr<void>> held_samples_{};
};

std::vector<std::uint8_t> PayloadOf(const EventSample& sample)
{
    return std::vector<std::uint8_t>(sample.payload.cbegin(),
                                     sample.payload.cbegin() + static_cast<std::ptrdiff_t>(sample.size));
}

/// Number of LoLa offers of the service instance that a LoLa consumer finds.
std::size_t FindLolaOffers(const std::uint16_t instance)
{
    auto specifier = score::mw::com::InstanceSpecifier::Create(LolaInstanceSpecifier(Service(), instance));
    if (!specifier.has_value())
    {
        return 0U;
    }
    const auto handles = score::mw::com::GenericProxy::FindService(specifier.value());
    return handles.has_value() ? handles.value().size() : 0U;
}

void Trace(const char* const requirements, const char* const given, const char* const when, const char* const then)
{
    ::testing::Test::RecordProperty("lobster-tracing", requirements);
    ::testing::Test::RecordProperty("given", given);
    ::testing::Test::RecordProperty("when", when);
    ::testing::Test::RecordProperty("then", then);
}

class NotificationHandlerTest : public ::testing::Test
{
  protected:
    static void SetUpTestSuite()
    {
        static const bool initialized = []() {
            score::mw::com::runtime::InitializeRuntime(
                score::mw::com::runtime::RuntimeConfiguration{score::filesystem::Path{WriteLolaConfig()}});
            return true;
        }();
        static_cast<void>(initialized);
    }

    void SetUp() override
    {
        UdpTransport() = FakeUdpTransport{};
    }

    NotificationHandler handler_{};
};

TEST_F(NotificationHandlerTest, OffersARemoteServiceAsLolaServiceInstance)
{
    Trace("RoutingModule.CR_RoutingModule_AddRemoteService,RoutingModule.CR_RoutingModule_LolaMapping",
          "a remote service instance whose LoLa service instance is configured",
          "the controller adds the remote service",
          "its local UDP port is opened and someip/<service>/<instance> is offered in LoLa with one LoLa event per "
          "SOME/IP event");

    ASSERT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kApplied);

    EXPECT_EQ(UdpTransport().opened_ports, (std::vector<unsigned short>{kUdpLocalPort}));
    auto specifier = score::mw::com::InstanceSpecifier::Create(LolaInstanceSpecifier(Service(), kConfiguredInstance));
    ASSERT_TRUE(specifier.has_value());
    const auto handles = score::mw::com::GenericProxy::FindService(specifier.value());
    ASSERT_TRUE(handles.has_value());
    ASSERT_EQ(handles.value().size(), 1U);
    auto lola_proxy = score::mw::com::GenericProxy::Create(handles.value().front());
    ASSERT_TRUE(lola_proxy.has_value());
    EXPECT_NE(lola_proxy.value().GetEvents().find(LolaEventName(kEvent)), lola_proxy.value().GetEvents().cend());
    EXPECT_NE(lola_proxy.value().GetEvents().find(LolaEventName(kOtherEvent)), lola_proxy.value().GetEvents().cend());
}

TEST_F(NotificationHandlerTest, RejectsRemoteServicesWhoseUdpRouteIsUsedByAnotherInstance)
{
    Trace("RoutingModule.CR_RoutingModule_UniqueReceiveRoute",
          "an offered remote service instance received on a local UDP port from a remote endpoint",
          "another instance of the service is added for the same local UDP port and remote endpoint, and then for "
          "another local UDP port",
          "the first addition is rejected without a LoLa offer, the second one is applied");
    ASSERT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kApplied);

    EXPECT_EQ(handler_.AddRemoteService(RemoteService(kOtherConfiguredInstance)), CommandResult::kRejected);
    EXPECT_EQ(FindLolaOffers(kOtherConfiguredInstance), 0U);

    EXPECT_EQ(handler_.AddRemoteService(RemoteService(kOtherConfiguredInstance, kUdpLocalPort + 1U)),
              CommandResult::kApplied);
    EXPECT_EQ(FindLolaOffers(kOtherConfiguredInstance), 1U);
    EXPECT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kApplied);
}

TEST_F(NotificationHandlerTest, StopsSearchingReceiveRoutesAtTheNextServiceId)
{
    Trace("RoutingModule.CR_RoutingModule_UniqueReceiveRoute",
          "two configured service IDs with a remote instance offered for the lower service ID",
          "another instance of the lower service ID is added with a distinct receive route",
          "the higher service ID does not cause a route collision and the new instance is offered");
    ASSERT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kApplied);
    ASSERT_EQ(handler_.AddRemoteService(
                  RemoteService(kConfiguredInstance, kUdpLocalPort + 1U, kUdpRemotePort + 1U, OtherService())),
              CommandResult::kApplied);

    EXPECT_EQ(
        handler_.AddRemoteService(RemoteService(kOtherConfiguredInstance, kUdpLocalPort + 2U, kUdpRemotePort + 2U)),
        CommandResult::kApplied);
}

TEST_F(NotificationHandlerTest, StopsTheLolaOfferOfRemovedRemoteServices)
{
    Trace("RoutingModule.CR_RoutingModule_RemoveRemoteService,RoutingModule.CR_RoutingModule_ControllerLoss",
          "two remote service instances offered in LoLa",
          "the controller removes one of them, adds it again, and then all routing state is removed",
          "the removed instance is not offered in LoLa until it is added again, and removing all routing state stops "
          "both LoLa offers");
    ASSERT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kApplied);
    ASSERT_EQ(handler_.AddRemoteService(RemoteService(kOtherConfiguredInstance, kUdpLocalPort + 1U)),
              CommandResult::kApplied);

    EXPECT_EQ(handler_.RemoveRemoteService(Instance(kConfiguredInstance)), CommandResult::kApplied);
    EXPECT_EQ(FindLolaOffers(kConfiguredInstance), 0U);
    EXPECT_EQ(FindLolaOffers(kOtherConfiguredInstance), 1U);

    EXPECT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kApplied);
    EXPECT_EQ(FindLolaOffers(kConfiguredInstance), 1U);

    handler_.RemoveAll();
    EXPECT_EQ(FindLolaOffers(kConfiguredInstance), 0U);
    EXPECT_EQ(FindLolaOffers(kOtherConfiguredInstance), 0U);
}

TEST_F(NotificationHandlerTest, RejectsInvalidRemoteServices)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidControl",
          "remote services with UDP port 0, without a configured LoLa service instance, or whose local UDP port "
          "cannot be opened",
          "the controller adds them",
          "each command is rejected and no LoLa service instance is offered");

    EXPECT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance, 0U)), CommandResult::kRejected);
    EXPECT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance, kUdpLocalPort, 0U)),
              CommandResult::kRejected);
    EXPECT_EQ(handler_.AddRemoteService(RemoteService(kUnconfiguredInstance)), CommandResult::kRejected);
    UdpTransport().open_error = EADDRINUSE;
    EXPECT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kRejected);

    EXPECT_EQ(FindLolaOffers(kConfiguredInstance), 0U);
    EXPECT_EQ(handler_.RemoveRemoteService(Instance(kConfiguredInstance)), CommandResult::kRejected);
}

TEST_F(NotificationHandlerTest, RejectsRemoteServiceWhenItsLolaInstanceIsAlreadyOffered)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidControl",
          "the configured LoLa service instance is already offered by another skeleton",
          "the controller adds a remote service for that instance",
          "the routing command is rejected and no routing service is offered");
    const auto specifier =
        score::mw::com::InstanceSpecifier::Create(LolaInstanceSpecifier(Service(), kConfiguredInstance));
    ASSERT_TRUE(specifier.has_value());
    const std::string first_event_name = LolaEventName(kEvent);
    const std::string second_event_name = LolaEventName(kOtherEvent);
    const std::array<score::mw::com::EventInfo, 2U> events{{
        {first_event_name, score::mw::com::DataTypeMetaInfo{sizeof(EventSample), alignof(EventSample)}},
        {second_event_name, score::mw::com::DataTypeMetaInfo{sizeof(EventSample), alignof(EventSample)}},
    }};
    score::mw::com::GenericSkeletonServiceElementInfo elements{};
    elements.events = score::cpp::span<const score::mw::com::EventInfo>{events.data(), events.size()};
    auto competing_skeleton = score::mw::com::GenericSkeleton::Create(specifier.value(), elements);
    ASSERT_TRUE(competing_skeleton.has_value());
    ASSERT_TRUE(competing_skeleton->OfferService().has_value());

    EXPECT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kRejected);
    EXPECT_EQ(FindLolaOffers(kConfiguredInstance), 1U);

    competing_skeleton->StopOfferService();
}

TEST_F(NotificationHandlerTest, AcceptsLocalSubscriptionsOnlyForEventsOfLolaOfferedServices)
{
    Trace(
        "RoutingModule.CR_RoutingModule_LocalSubscriptions,RoutingModule.CR_RoutingModule_SubscriptionRenewal,"
        "RoutingModule.CR_RoutingModule_InvalidControl",
        "a remote service instance offered in LoLa",
        "local subscriptions are added twice, removed, and added for an unknown event, an unknown instance and "
        "after the remote service is removed",
        "repeated additions and the removal are applied, the other subscriptions are rejected");
    ASSERT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kApplied);

    EXPECT_EQ(handler_.AddLocalSubscription(Instance(kConfiguredInstance), kEvent), CommandResult::kApplied);
    EXPECT_EQ(handler_.AddLocalSubscription(Instance(kConfiguredInstance), kEvent), CommandResult::kApplied);
    EXPECT_EQ(handler_.RemoveLocalSubscription(Instance(kConfiguredInstance), kEvent), CommandResult::kApplied);
    EXPECT_EQ(handler_.AddLocalSubscription(Instance(kConfiguredInstance), 0x8003U), CommandResult::kRejected);
    EXPECT_EQ(handler_.AddLocalSubscription(Instance(kOtherConfiguredInstance), kEvent), CommandResult::kRejected);
    EXPECT_EQ(handler_.RemoveLocalSubscription(Instance(kOtherConfiguredInstance), kEvent), CommandResult::kRejected);

    ASSERT_EQ(handler_.RemoveRemoteService(Instance(kConfiguredInstance)), CommandResult::kApplied);
    EXPECT_EQ(handler_.AddLocalSubscription(Instance(kConfiguredInstance), kEvent), CommandResult::kRejected);
}

TEST_F(NotificationHandlerTest, RejectsUnsupportedTransmitCommands)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidControl",
          "a routing module whose transmit commands are not supported",
          "the controller requests local service and remote subscription changes",
          "each unsupported command is rejected");
    EXPECT_EQ(handler_.AddLocalService(RemoteService(kConfiguredInstance)), CommandResult::kRejected);
    EXPECT_EQ(handler_.RemoveLocalService(Instance(kConfiguredInstance)), CommandResult::kRejected);
    EXPECT_EQ(handler_.AddRemoteSubscription(Instance(kConfiguredInstance), kEvent, kUdpSender),
              CommandResult::kRejected);
    EXPECT_EQ(handler_.RemoveRemoteSubscription(Instance(kConfiguredInstance), kEvent, kUdpSender),
              CommandResult::kRejected);
}

class NotificationHandlerReceiveTest : public NotificationHandlerTest
{
  protected:
    void SetUp() override
    {
        NotificationHandlerTest::SetUp();
        ASSERT_EQ(handler_.AddRemoteService(RemoteService(kConfiguredInstance)), CommandResult::kApplied);
        ASSERT_EQ(handler_.AddLocalSubscription(Instance(kConfiguredInstance), kEvent), CommandResult::kApplied);
        subscriber_.emplace(kConfiguredInstance, kEvent);
    }

    void TearDown() override
    {
        subscriber_.reset();
    }

    std::optional<LolaSubscriber> subscriber_{};
};

TEST_F(NotificationHandlerReceiveTest, ForwardsTheNotificationMetadataAndPayloadIntoLola)
{
    Trace(
        "RoutingModule.CR_RoutingModule_ReceiveMetadata,RoutingModule.CR_RoutingModule_PayloadPreservation,"
        "RoutingModule.CR_RoutingModule_DirectReceiveCopy,RoutingModule.CR_RoutingModule_LolaMapping",
        "a locally subscribed SOME/IP event of a remote service offered in LoLa and a fake UDP packet",
        "a UDP datagram with a notification of the event is received",
        "a LoLa sample carries payload length, session ID, interface version, message type and the unchanged "
        "payload, copied once from the fake packet");
    const std::vector<std::uint8_t> datagram = Datagram(Notification{Service(), kEvent, Payload(5U), 0x0042U, 0x07U});

    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(datagram));

    const auto samples = subscriber_->Receive();
    ASSERT_EQ(samples.size(), 1U);
    EXPECT_EQ(samples.front().size, 5U);
    EXPECT_EQ(samples.front().session, 0x0042U);
    EXPECT_EQ(samples.front().interface_version, 0x07U);
    EXPECT_EQ(samples.front().message_type, 0x02U);
    EXPECT_EQ(PayloadOf(samples.front()), Payload(5U));
    // One header copy by the parser and one payload copy into the LoLa sample.
    EXPECT_EQ(UdpTransport().packet_copies, 2U);
}

TEST_F(NotificationHandlerReceiveTest, SelectsTheMatchingReceivePathAmongServiceInstances)
{
    Trace("RoutingModule.CR_RoutingModule_ReceiveRouteMatch",
          "two LoLa-offered instances of one SOME/IP service with distinct UDP receive routes",
          "a notification arrives on the second instance's local port and sender endpoint",
          "only the second instance receives the notification");
    constexpr std::uint16_t second_local_port = kUdpLocalPort + 1U;
    constexpr std::uint16_t second_remote_port = kUdpRemotePort + 1U;
    ASSERT_EQ(handler_.AddRemoteService(RemoteService(kOtherConfiguredInstance, second_local_port, second_remote_port)),
              CommandResult::kApplied);
    ASSERT_EQ(handler_.AddLocalSubscription(Instance(kOtherConfiguredInstance), kEvent), CommandResult::kApplied);
    LolaSubscriber second_subscriber{kOtherConfiguredInstance, kEvent};
    const auto datagram = Datagram(Notification{Service(), kEvent, Payload(2U)});

    handler_.OnUdpDatagram(second_local_port, Endpoint{kUdpSender.address, second_remote_port}, Reader(datagram));

    const auto samples = second_subscriber.Receive();
    ASSERT_EQ(samples.size(), 1U);
    EXPECT_EQ(PayloadOf(samples.front()), Payload(2U));
    EXPECT_TRUE(subscriber_->Receive().empty());
}

TEST_F(NotificationHandlerReceiveTest, DropsFailedHeaderReadAndInvalidHeader)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidFraming",
          "a locally subscribed event and a fake UDP packet",
          "the header copy fails or a complete header has an invalid protocol version",
          "neither notification is forwarded");
    const auto datagram = Datagram(Notification{Service(), kEvent, Payload(3U)});
    UdpTransport().fail_packet_copy = 1U;
    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(datagram));

    auto invalid_header = datagram;
    invalid_header.at(12U) = 2U;
    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(invalid_header));
    EXPECT_TRUE(subscriber_->Receive().empty());
}

TEST_F(NotificationHandlerReceiveTest, DropsNotificationWhenPayloadCopyFails)
{
    Trace("RoutingModule.CR_RoutingModule_DirectReceiveCopy",
          "a subscribed event and a fake UDP packet whose payload read fails",
          "a notification is received, then a later payload read succeeds",
          "the failed payload is not published and the later notification is delivered unchanged");
    const auto datagram = Datagram(Notification{Service(), kEvent, Payload(3U)});
    UdpTransport().fail_packet_copy = 2U;

    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(datagram));
    EXPECT_TRUE(subscriber_->Receive().empty());

    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(datagram));
    const auto samples = subscriber_->Receive();
    ASSERT_EQ(samples.size(), 1U);
    EXPECT_EQ(PayloadOf(samples.front()), Payload(3U));
}

TEST_F(NotificationHandlerReceiveTest, ForwardsConcatenatedEmptyAndMaximumSizeNotifications)
{
    Trace(
        "RoutingModule.CR_RoutingModule_ConcatenatedMessages,RoutingModule.CR_RoutingModule_EmptyPayload,"
        "RoutingModule.CR_RoutingModule_PayloadCapacity,RoutingModule.CR_RoutingModule_NotificationOnly",
        "a locally subscribed SOME/IP event of a remote service offered in LoLa",
        "a UDP datagram with an empty notification, a response, a 1400-byte notification and a 3-byte notification "
        "is received",
        "the three notifications are forwarded into LoLa in order and the response is skipped");
    std::vector<std::uint8_t> datagram{};
    Append(datagram, Notification{Service(), kEvent, {}});
    Append(datagram, Notification{Service(), kEvent, Payload(4U), 0x1234U, 0x03U, 0x80U});
    Append(datagram, Notification{Service(), kEvent, Payload(kMaxPayloadSize)});
    Append(datagram, Notification{Service(), kEvent, Payload(3U, 0xA0U)});

    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(datagram));

    const auto samples = subscriber_->Receive();
    ASSERT_EQ(samples.size(), 3U);
    EXPECT_EQ(samples.at(0U).size, 0U);
    EXPECT_EQ(PayloadOf(samples.at(1U)), Payload(kMaxPayloadSize));
    EXPECT_EQ(PayloadOf(samples.at(2U)), Payload(3U, 0xA0U));
}

TEST_F(NotificationHandlerReceiveTest, ForwardsOnlyNotificationsMatchingAnActiveReceivePathAndLocalSubscription)
{
    Trace(
        "RoutingModule.CR_RoutingModule_ReceiveRouteMatch,RoutingModule.CR_RoutingModule_LocalSubscriptions,"
        "RoutingModule.CR_RoutingModule_RemoveRemoteService",
        "a locally subscribed SOME/IP event of a remote service offered in LoLa",
        "notifications arrive on another local UDP port, from another IPv4 address or UDP port, for another service, "
        "for an event the remote service does not have or a not subscribed event, after the local subscription is "
        "removed, and after the remote service is removed",
        "none of them is forwarded into LoLa, while a matching notification is");
    const auto datagram = Datagram(Notification{Service(), kEvent, Payload(2U)});
    const Endpoint other_address{routing_control_protocol::Ipv4Address{10U, 0U, 3U, 9U}, kUdpRemotePort};
    const Endpoint other_port{kUdpSender.address, static_cast<std::uint16_t>(kUdpRemotePort + 1U)};

    handler_.OnUdpDatagram(kUdpLocalPort + 1U, kUdpSender, Reader(datagram));
    handler_.OnUdpDatagram(kUdpLocalPort, other_address, Reader(datagram));
    handler_.OnUdpDatagram(kUdpLocalPort, other_port, Reader(datagram));
    handler_.OnUdpDatagram(kUdpLocalPort,
                           kUdpSender,
                           Reader(Datagram(Notification{static_cast<std::uint16_t>(Service() + 1U), kEvent, {}})));
    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(Datagram(Notification{Service(), kOtherEvent, {}})));
    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(Datagram(Notification{Service(), kUnknownEvent, {}})));
    EXPECT_TRUE(subscriber_->Receive().empty());

    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(datagram));
    EXPECT_EQ(subscriber_->Receive().size(), 1U);

    ASSERT_EQ(handler_.RemoveLocalSubscription(Instance(kConfiguredInstance), kEvent), CommandResult::kApplied);
    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(datagram));
    EXPECT_TRUE(subscriber_->Receive().empty());

    ASSERT_EQ(handler_.AddLocalSubscription(Instance(kConfiguredInstance), kEvent), CommandResult::kApplied);
    ASSERT_EQ(handler_.RemoveRemoteService(Instance(kConfiguredInstance)), CommandResult::kApplied);
    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(datagram));
    EXPECT_TRUE(subscriber_->Receive().empty());
}

TEST_F(NotificationHandlerReceiveTest, LimitsEachDropReasonIndependently)
{
    Trace("RoutingModule.CR_RoutingModule_ErrorLogRate",
          "a subscribed event and fixed per-reason RX drop log limiters",
          "bursts of notifications with different drop reasons are received",
          "one warning or error is logged per reason and no sample is forwarded");
    ::testing::NiceMock<score::mw::log::RecorderMock> recorder{};
    const score::mw::log::SlotHandle handle{42};
    EXPECT_CALL(recorder, StartRecord("SRTM", score::mw::log::LogLevel::kWarn))
        .Times(4)
        .WillRepeatedly(::testing::Return(handle));
    EXPECT_CALL(recorder, StartRecord("SRTM", score::mw::log::LogLevel::kError))
        .Times(1)
        .WillRepeatedly(::testing::Return(handle));
    EXPECT_CALL(recorder, LogUint64(handle, 0U)).Times(7);
    EXPECT_CALL(recorder, LogUint64(handle, kMaxPayloadSize + 1U)).Times(1);
    score::mw::log::SetLogRecorder(&recorder);

    const auto unmatched = Datagram(Notification{static_cast<std::uint16_t>(Service() + 1U), kEvent, {}});
    const auto response = Datagram(Notification{Service(), kEvent, {}, 0x1234U, 0x03U, 0x80U});
    const auto unknown_event = Datagram(Notification{Service(), kUnknownEvent, {}});
    const auto unsubscribed = Datagram(Notification{Service(), kOtherEvent, {}});
    const auto oversized = Datagram(Notification{Service(), kEvent, Payload(kMaxPayloadSize + 1U)});
    for (unsigned int count = 0U; count < 10U; ++count)
    {
        handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(unmatched));
        handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(response));
        handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(unknown_event));
        handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(unsubscribed));
        handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(oversized));
    }
    score::mw::log::SetLogRecorder(nullptr);
    EXPECT_TRUE(subscriber_->Receive().empty());
}

TEST_F(NotificationHandlerReceiveTest, DropsOversizedPayloadsAndTheRestOfMalformedDatagrams)
{
    Trace("RoutingModule.CR_RoutingModule_OversizedPayload,RoutingModule.CR_RoutingModule_InvalidFraming",
          "a locally subscribed SOME/IP event of a remote service offered in LoLa",
          "a datagram with a 1401-byte notification followed by a valid one, and a datagram with a valid notification "
          "followed by a truncated SOME/IP header are received",
          "the oversized notification and the truncated rest are dropped, the valid notifications are forwarded");
    std::vector<std::uint8_t> oversized{};
    Append(oversized, Notification{Service(), kEvent, Payload(kMaxPayloadSize + 1U)});
    Append(oversized, Notification{Service(), kEvent, Payload(1U)});
    std::vector<std::uint8_t> malformed = Datagram(Notification{Service(), kEvent, Payload(2U)});
    malformed.insert(malformed.end(), {0x12U, 0x34U, 0x80U});

    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(oversized));
    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(malformed));

    const auto samples = subscriber_->Receive();
    ASSERT_EQ(samples.size(), 2U);
    EXPECT_EQ(samples.at(0U).size, 1U);
    EXPECT_EQ(samples.at(1U).size, 2U);
}

TEST_F(NotificationHandlerReceiveTest, DropsNotificationsWhileNoLolaSampleSlotIsFree)
{
    Trace("RoutingModule.CR_RoutingModule_ReceiveSlotExhaustion",
          "a LoLa consumer that holds all sample slots of a locally subscribed event",
          "another notification of the event is received, then the consumer releases its samples and one more "
          "notification is received",
          "the notification without free slot is dropped and the one after the release is forwarded");
    for (std::uint8_t index = 0U; index < kLolaSampleSlots; ++index)
    {
        handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(Datagram(Notification{Service(), kEvent, {index}})));
    }
    ASSERT_EQ(subscriber_->Receive().size(), kLolaSampleSlots);

    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(Datagram(Notification{Service(), kEvent, {0xEEU}})));
    EXPECT_TRUE(subscriber_->Receive().empty());

    subscriber_->ReleaseSamples();
    handler_.OnUdpDatagram(kUdpLocalPort, kUdpSender, Reader(Datagram(Notification{Service(), kEvent, {0xFFU}})));
    const auto samples = subscriber_->Receive();
    ASSERT_EQ(samples.size(), 1U);
    EXPECT_EQ(PayloadOf(samples.front()), (std::vector<std::uint8_t>{0xFFU}));
}

}  // namespace
}  // namespace score::someip::routing_module
