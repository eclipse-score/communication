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
#include "score/someip/routing_module/iosock_bridge/iosock_bridge.h"

#include "score/someip/routing_module/iosock_transport/iosock_transport.h"
#include "score/someip/routing_module/iosock_transport/log.h"
#include "score/someip/routing_module/lola_mapping/lola_mapping.h"

#include "score/filesystem/path.h"
#include "score/message_passing/mock/server_connection_mock.h"
#include "score/message_passing/mock/server_factory_mock.h"
#include "score/message_passing/mock/server_mock.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"
#include "score/mw/log/logging.h"
#include "score/mw/log/recorder_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

std::size_t& Wakeups()
{
    static std::size_t wakeups{0U};
    return wakeups;
}

}  // namespace

/// Test doubles of the io-sock module; a received packet is a std::vector<std::uint8_t> here.
extern "C" int someip_iosock_open_udp_socket(const unsigned short /*port*/)
{
    return 0;
}

extern "C" void someip_iosock_copy_packet(const void* const packet,
                                          const unsigned int offset,
                                          const unsigned int size,
                                          void* const destination)
{
    const auto& bytes = *static_cast<const std::vector<std::uint8_t>*>(packet);
    std::copy_n(bytes.cbegin() + static_cast<std::ptrdiff_t>(offset), size, static_cast<std::uint8_t*>(destination));
}

extern "C" void someip_iosock_wakeup(void)
{
    ++Wakeups();
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

constexpr std::uint16_t kInstance{0x0001U};
constexpr std::uint16_t kEvent{0x8001U};
constexpr std::uint16_t kUdpLocalPort{30501U};
const Endpoint kUdpSender{routing_control_protocol::Ipv4Address{10U, 0U, 3U, 2U}, 30502U};

/// LoLa's shared memory and discovery paths are global, so concurrent runs of LoLa tests use distinct services.
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
    const std::filesystem::path path = directory / ("iosock_bridge_test_" + std::to_string(::getpid()) + ".json");
    std::ofstream{path}
        << R"({"serviceTypes": [{"serviceTypeName": "/someip/service", "version": {"major": 1, "minor": 0}, )"
           R"("bindings": [{"binding": "SHM", "serviceId": )"
        << Service() << R"(, "events": [{"eventName": ")" << LolaEventName(kEvent) << R"(", "eventId": 1}]}]}], )"
        << R"("serviceInstances": [{"instanceSpecifier": ")" << LolaInstanceSpecifier(Service(), kInstance)
        << R"(", "serviceTypeName": "/someip/service", "version": {"major": 1, "minor": 0}, "instances": [{)"
           R"("instanceId": )"
        << kInstance << R"(, "asil-level": "QM", "binding": "SHM", "events": [{"eventName": ")" << LolaEventName(kEvent)
        << R"(", "numberOfSampleSlots": 2, "maxSubscribers": 1}]}]}]})";
    return path.string();
}

routing_control_protocol::Message AddRemoteService()
{
    routing_control_protocol::Message message{};
    message.opcode = routing_control_protocol::Opcode::kAddRemoteService;
    message.service = Service();
    message.instance = kInstance;
    message.local_port = kUdpLocalPort;
    message.remote_ip = kUdpSender.address;
    message.remote_port = kUdpSender.port;
    message.events.push_back(kEvent);
    return message;
}

routing_control_protocol::Message AddLocalSubscription()
{
    routing_control_protocol::Message message{};
    message.opcode = routing_control_protocol::Opcode::kAddLocalSubscription;
    message.service = Service();
    message.instance = kInstance;
    message.event = kEvent;
    return message;
}

/// A UDP datagram with one SOME/IP notification of kEvent.
std::vector<std::uint8_t> NotificationDatagram(const std::vector<std::uint8_t>& payload)
{
    const auto length = static_cast<std::uint32_t>(payload.size() + 8U);
    std::vector<std::uint8_t> datagram{static_cast<std::uint8_t>(Service() >> 8U),
                                       static_cast<std::uint8_t>(Service()),
                                       static_cast<std::uint8_t>(kEvent >> 8U),
                                       static_cast<std::uint8_t>(kEvent),
                                       static_cast<std::uint8_t>(length >> 24U),
                                       static_cast<std::uint8_t>(length >> 16U),
                                       static_cast<std::uint8_t>(length >> 8U),
                                       static_cast<std::uint8_t>(length),
                                       0x00U,
                                       0x00U,
                                       0x00U,
                                       0x01U,
                                       0x01U,
                                       0x01U,
                                       0x02U,
                                       0x00U};
    datagram.insert(datagram.end(), payload.cbegin(), payload.cend());
    return datagram;
}

std::size_t FindLolaOffers()
{
    auto specifier = score::mw::com::InstanceSpecifier::Create(LolaInstanceSpecifier(Service(), kInstance));
    if (!specifier.has_value())
    {
        return 0U;
    }
    const auto handles = score::mw::com::GenericProxy::FindService(specifier.value());
    return handles.has_value() ? handles.value().size() : 0U;
}

/// LoLa consumer of kEvent.
class LolaSubscriber
{
  public:
    LolaSubscriber()
    {
        auto specifier = score::mw::com::InstanceSpecifier::Create(LolaInstanceSpecifier(Service(), kInstance));
        EXPECT_TRUE(specifier.has_value());
        auto handles = score::mw::com::GenericProxy::FindService(specifier.value());
        EXPECT_TRUE(handles.has_value() && (handles.value().size() == 1U));
        auto proxy = score::mw::com::GenericProxy::Create(handles.value().front());
        EXPECT_TRUE(proxy.has_value());
        lola_proxy_.emplace(std::move(proxy).value());
        lola_event_ = &lola_proxy_->GetEvents().find(LolaEventName(kEvent))->second;
        EXPECT_TRUE(lola_event_->Subscribe(1U).has_value());
    }

    // GenericProxy does not unsubscribe its events on destruction; LoLa aborts if they are still subscribed.
    ~LolaSubscriber()
    {
        lola_event_->Unsubscribe();
    }

    LolaSubscriber(const LolaSubscriber&) = delete;
    LolaSubscriber& operator=(const LolaSubscriber&) = delete;
    LolaSubscriber(LolaSubscriber&&) = delete;
    LolaSubscriber& operator=(LolaSubscriber&&) = delete;

    std::vector<std::vector<std::uint8_t>> ReceivePayloads()
    {
        std::vector<std::vector<std::uint8_t>> payloads{};
        static_cast<void>(lola_event_->GetNewSamples(
            [&payloads](score::mw::com::SamplePtr<void> sample) noexcept {
                const auto& event_sample = *static_cast<const EventSample*>(sample.Get());
                payloads.emplace_back(event_sample.payload.cbegin(),
                                      event_sample.payload.cbegin() + static_cast<std::ptrdiff_t>(event_sample.size));
            },
            1U));
        return payloads;
    }

  private:
    std::optional<score::mw::com::GenericProxy> lola_proxy_{};
    score::mw::com::GenericProxyEvent* lola_event_{nullptr};
};

void Trace(const char* const requirements, const char* const given, const char* const when, const char* const then)
{
    ::testing::Test::RecordProperty("lobster-tracing", requirements);
    ::testing::Test::RecordProperty("given", given);
    ::testing::Test::RecordProperty("when", when);
    ::testing::Test::RecordProperty("then", then);
}

class IoSockBridgeTest : public ::testing::Test
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
        Wakeups() = 0U;
        ON_CALL(*server_, StartListening(_, _, _, _))
            .WillByDefault([this](ConnectCallback connect,
                                  DisconnectCallback /*disconnect*/,
                                  MessageCallback command,
                                  MessageCallback /*request*/) -> score::cpp::expected_blank<score::os::Error> {
                connect_ = std::move(connect);
                command_ = std::move(command);
                return {};
            });
        ON_CALL(server_factory_, Create(_, _)).WillByDefault(Return(::testing::ByMove(std::move(server_))));
        ON_CALL(sd_daemon_, GetClientIdentity()).WillByDefault(ReturnRef(sd_daemon_identity_));
        ON_CALL(sd_daemon_, GetUserData()).WillByDefault(ReturnRef(sd_daemon_user_data_));
    }

    void StartAndConnectSdDaemon()
    {
        ASSERT_TRUE(bridge_.Start().has_value());
        auto user_data = connect_(sd_daemon_);
        ASSERT_TRUE(user_data.has_value());
        sd_daemon_user_data_ = std::move(user_data).value();
    }

    void Send(const routing_control_protocol::Message& command)
    {
        std::array<std::uint8_t, routing_control_protocol::kMaxMessageSize> buffer{};
        const auto size = routing_control_protocol::Encode(command, buffer);
        ASSERT_TRUE(size.has_value());
        EXPECT_TRUE(
            command_(sd_daemon_, score::cpp::span<const std::uint8_t>{buffer.data(), size.value()}).has_value());
    }

    NiceMock<ServerFactoryMock> server_factory_{};
    score::cpp::pmr::unique_ptr<NiceMock<ServerMock>> server_{
        score::cpp::pmr::make_unique<NiceMock<ServerMock>>(score::cpp::pmr::get_default_resource())};
    NiceMock<ServerConnectionMock> sd_daemon_{};
    ClientIdentity sd_daemon_identity_{100, 1000U, 1000U};
    UserData sd_daemon_user_data_{};
    ConnectCallback connect_{};
    MessageCallback command_{};

    IoSockBridge bridge_{server_factory_};
};

TEST_F(IoSockBridgeTest, AppliesRoutingCommandsOnPollAndForwardsReceivedNotificationsIntoLola)
{
    Trace("RoutingModule.CR_RoutingModule_ControlOrdering,RoutingModule.CR_RoutingModule_LocalSubscriptions",
          "a started io-sock bridge with a connected SD daemon",
          "the SD daemon adds a remote service and a local subscription of one of its events, the module's kernel "
          "thread polls, and a UDP datagram with a notification of the event is received",
          "each routing command wakes the kernel thread, the commands take effect with the poll only, and the "
          "notification is published in LoLa");
    StartAndConnectSdDaemon();

    Send(AddRemoteService());
    Send(AddLocalSubscription());

    EXPECT_EQ(Wakeups(), 2U);
    EXPECT_EQ(FindLolaOffers(), 0U);

    bridge_.Poll();

    ASSERT_EQ(FindLolaOffers(), 1U);
    LolaSubscriber subscriber{};
    const std::vector<std::uint8_t> payload{0xAAU, 0xBBU, 0xCCU};
    const auto datagram = NotificationDatagram(payload);
    bridge_.OnUdpDatagram(kUdpLocalPort, kUdpSender, &datagram, static_cast<std::uint32_t>(datagram.size()));

    const auto payloads = subscriber.ReceivePayloads();
    ASSERT_EQ(payloads.size(), 1U);
    EXPECT_EQ(payloads.front(), payload);
}

TEST(ModuleConfigDirectoryTest, IsTheEtcDirectoryNextToTheModuleDirectory)
{
    Trace("RoutingModule.CR_RoutingModule_IoSockModule",
          "an io-sock module installed as <prefix>/bin/mods-someip-routing.so",
          "its configuration directory is determined",
          "it is <prefix>/etc, also for a module path relative to the working directory");

    EXPECT_EQ(ModuleConfigDirectory("/opt/someip/bin/mods-someip-routing.so"), "/opt/someip/bin/../etc");
    EXPECT_EQ(ModuleConfigDirectory("mods-someip-routing.so"), "./../etc");
}

TEST(IoSockLoggingTest, FormatsAndMapsCLogSeveritiesToMwLog)
{
    ::testing::StrictMock<score::mw::log::RecorderMock> recorder{};
    score::mw::log::SetLogRecorder(&recorder);
    const score::mw::log::SlotHandle handle{42};
    const std::array<std::pair<someip_log_severity, score::mw::log::LogLevel>, 4U> levels{{
        {SOMEIP_LOG_ERROR, score::mw::log::LogLevel::kError},
        {SOMEIP_LOG_WARNING, score::mw::log::LogLevel::kWarn},
        {SOMEIP_LOG_INFO, score::mw::log::LogLevel::kInfo},
        {SOMEIP_LOG_DEBUG, score::mw::log::LogLevel::kDebug},
    }};
    for (const auto& level : levels)
    {
        EXPECT_CALL(recorder, StartRecord("SRTM", level.second)).WillOnce(Return(handle));
        EXPECT_CALL(recorder, LogStringView(handle, "port 42")).Times(1);
        EXPECT_CALL(recorder, StopRecord(handle)).Times(1);
        someip_log(level.first, "port %u", 42U);
    }
    const std::string long_message(300U, 'x');
    EXPECT_CALL(recorder, StartRecord("SRTM", score::mw::log::LogLevel::kError)).WillOnce(Return(handle));
    EXPECT_CALL(recorder, LogStringView(handle, std::string(255U, 'x'))).Times(1);
    EXPECT_CALL(recorder, StopRecord(handle)).Times(1);
    someip_log(SOMEIP_LOG_ERROR, "%s", long_message.c_str());
    score::mw::log::SetLogRecorder(nullptr);
}

}  // namespace
}  // namespace score::someip::routing_module
