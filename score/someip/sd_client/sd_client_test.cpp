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
#include "score/someip/sd_client/sd_client.h"

#include "score/message_passing/i_client_connection.h"
#include "score/message_passing/mock/client_connection_mock.h"
#include "score/message_passing/mock/client_factory_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace score::someip_sd
{
namespace
{

using score::message_passing::ClientConnectionMock;
using score::message_passing::ClientConnectionMockFacade;
using score::message_passing::ClientFactoryMock;
using score::message_passing::IClientConnection;
using ::testing::_;
using ::testing::ByMove;
using ::testing::NiceMock;
using ::testing::Return;

using Reply = score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error>;
using Bytes = std::array<std::uint8_t, protocol::kMessageSize>;

/// Only a hang guard: every wait below is expected to complete long before it.
constexpr auto kHangGuard = std::chrono::seconds{5};

MATCHER(IsNonBlockingClientConfig, "client config with truly asynchronous, queued sends")
{
    return (arg.max_async_replies == 1U) && (arg.max_queued_sends == 16U) && arg.fully_ordered && arg.truly_async &&
           !arg.sync_first_connect;
}

score::cpp::span<const std::uint8_t> AsSpan(const Bytes& buffer)
{
    return score::cpp::span<const std::uint8_t>{buffer.data(), buffer.size()};
}

Bytes ToBytes(const score::cpp::span<const std::uint8_t> data)
{
    Bytes bytes{};
    EXPECT_EQ(data.size(), bytes.size());
    std::copy_n(data.data(), std::min(bytes.size(), static_cast<std::size_t>(data.size())), bytes.begin());
    return bytes;
}

Bytes RegisterReply(const std::uint16_t client_id)
{
    protocol::Message response{};
    response.opcode = protocol::Opcode::kRegister;
    response.client_id = client_id;
    return protocol::Encode(response);
}

void DeliverReply(IClientConnection::ReplyCallback& callback, const Bytes& buffer)
{
    callback(Reply{AsSpan(buffer)});
}

/// One-shot event with a hang-guarded wait.
class Signal
{
  public:
    void Set()
    {
        {
            const std::lock_guard lock{mutex_};
            set_ = true;
        }
        condition_.notify_all();
    }

    bool WaitFor()
    {
        std::unique_lock lock{mutex_};
        return condition_.wait_for(lock, kHangGuard, [this]() {
            return set_;
        });
    }

    bool IsSet()
    {
        const std::lock_guard lock{mutex_};
        return set_;
    }

  private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool set_{false};
};

struct Connection
{
    ClientConnectionMock mock;
    IClientConnection::StateCallback state_callback;
    IClientConnection::NotifyCallback notify_callback;

    score::cpp::pmr::unique_ptr<IClientConnection> MakeFacade()
    {
        return score::cpp::pmr::make_unique<NiceMock<ClientConnectionMockFacade>>(
            score::cpp::pmr::new_delete_resource(), mock);
    }

    void ExpectStartReady()
    {
        EXPECT_CALL(mock, Start(_, _)).WillOnce([this](auto state_cb, auto notify_cb) {
            state_callback = std::move(state_cb);
            notify_callback = std::move(notify_cb);
            state_callback(IClientConnection::State::kStarting);
            state_callback(IClientConnection::State::kReady);
        });
    }

    void ExpectSynchronousStop()
    {
        EXPECT_CALL(mock, Stop()).WillOnce([this]() {
            state_callback(IClientConnection::State::kStopping);
            state_callback(IClientConnection::State::kStopped);
        });
    }

    void ExpectRegistration(const std::uint16_t client_id)
    {
        EXPECT_CALL(mock, SendWithCallback(_, _)).WillOnce([client_id](auto, auto callback) {
            DeliverReply(callback, RegisterReply(client_id));
            return score::cpp::expected_blank<score::os::Error>{};
        });
    }

    void ExpectReadySession(const std::uint16_t client_id)
    {
        ExpectStartReady();
        ExpectRegistration(client_id);
        ExpectSynchronousStop();
    }
};

class SdClientTest : public ::testing::Test
{
  protected:
    void ExpectOneConnection()
    {
        EXPECT_CALL(factory_mock_, Create(_, IsNonBlockingClientConfig()))
            .WillOnce(Return(ByMove(first_.MakeFacade())));
    }

    void ExpectTwoConnections()
    {
        EXPECT_CALL(factory_mock_, Create(_, IsNonBlockingClientConfig()))
            .WillOnce(Return(ByMove(first_.MakeFacade())))
            .WillOnce(Return(ByMove(second_.MakeFacade())));
    }

    // Declared before the factory mock: unused facades held by the factory expectations reference these mocks.
    Connection first_;
    Connection second_;
    ClientFactoryMock factory_mock_;
};

// Verifies: cpp-9 — the client config requests truly asynchronous sends (asserted by the factory matcher).
TEST_F(SdClientTest, ConnectRegistersOnceAndSendsAllControlOperations)
{
    ExpectOneConnection();
    first_.ExpectStartReady();
    first_.ExpectSynchronousStop();
    SdClient client{factory_mock_, {}, {}};
    constexpr std::uint16_t client_id{0x1234U};
    EXPECT_CALL(first_.mock, SendWithCallback(_, _)).WillOnce([client_id](auto request, auto callback) {
        const auto register_request = protocol::Decode(request.data(), request.size());
        EXPECT_TRUE(register_request.has_value());
        if (register_request.has_value())
        {
            EXPECT_EQ(register_request->opcode, protocol::Opcode::kRegister);
        }
        DeliverReply(callback, RegisterReply(client_id));
        return score::cpp::expected_blank<score::os::Error>{};
    });

    ASSERT_EQ(client.Connect(std::chrono::seconds{1}), client_id);
    ASSERT_EQ(client.Connect(std::chrono::seconds{1}), client_id);

    std::vector<protocol::Opcode> sent_opcodes;
    EXPECT_CALL(first_.mock, Send(_)).Times(7).WillRepeatedly([&sent_opcodes](auto message) {
        const auto decoded = protocol::Decode(message.data(), message.size());
        EXPECT_TRUE(decoded.has_value());
        if (!decoded.has_value())
        {
            return score::cpp::expected_blank<score::os::Error>{};
        }
        if (decoded->opcode == protocol::Opcode::kRequestService)
        {
            EXPECT_EQ(decoded->service, 1U);
            EXPECT_EQ(decoded->instance, 2U);
            EXPECT_EQ(decoded->major, 3U);
            EXPECT_EQ(decoded->minor, 4U);
        }
        sent_opcodes.push_back(decoded->opcode);
        return score::cpp::expected_blank<score::os::Error>{};
    });

    EXPECT_TRUE(client.RequestService(1U, 2U, 3U, 4U));
    EXPECT_TRUE(client.RequestService(ServiceDetails{1U, 2U, 3U, 4U}));
    EXPECT_TRUE(client.ReleaseService(1U, 2U));
    EXPECT_TRUE(client.Subscribe(1U, 2U, 3U, 4U));
    EXPECT_TRUE(client.Unsubscribe(1U, 2U, 3U));
    EXPECT_TRUE(client.OfferService(1U, 2U, 3U, 4U));
    EXPECT_TRUE(client.StopOfferService(1U, 2U));

    const std::vector<protocol::Opcode> expected_opcodes{
        protocol::Opcode::kRequestService,
        protocol::Opcode::kRequestService,
        protocol::Opcode::kReleaseService,
        protocol::Opcode::kSubscribe,
        protocol::Opcode::kUnsubscribe,
        protocol::Opcode::kOfferService,
        protocol::Opcode::kStopOfferService,
    };
    EXPECT_EQ(sent_opcodes, expected_opcodes);
}

// Verifies: cpp-9 — a send rejected by the transport (e.g. full client-side queue) is reported as failure.
TEST_F(SdClientTest, RejectedSendIsReportedAsFailure)
{
    ExpectOneConnection();
    first_.ExpectReadySession(0x10U);
    SdClient client{factory_mock_, {}, {}};
    ASSERT_TRUE(client.Connect(std::chrono::seconds{1}).has_value());
    EXPECT_CALL(first_.mock, Send(_)).WillOnce([](auto) {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN));
    });

    EXPECT_FALSE(client.RequestService(1U, 2U, 3U, 4U));
}

// Verifies: tests-6 — the encoding matches the fixed 16-byte little-endian layout documented in sd_protocol.h:
// [0] version, [1] opcode, [2..3] service, [4..5] instance, [6] major, [7] flag, [8..11] minor, [12..13] eventgroup,
// [14..15] client id.
TEST_F(SdClientTest, EncodesAndDecodesTheFixedLittleEndianLayout)
{
    ExpectOneConnection();
    first_.ExpectStartReady();
    first_.ExpectSynchronousStop();
    std::vector<std::pair<std::uint16_t, std::uint16_t>> available_services;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> unavailable_services;
    SdClient client{
        factory_mock_,
        [&available_services, &unavailable_services](std::uint16_t service, std::uint16_t instance, bool available) {
            (available ? available_services : unavailable_services).emplace_back(service, instance);
        },
        {}};

    Bytes register_request{};
    constexpr Bytes kRegisterReply{
        0x01U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xCDU, 0xABU};
    EXPECT_CALL(first_.mock, SendWithCallback(_, _))
        .WillOnce([&register_request, &kRegisterReply](auto request, auto callback) {
            register_request = ToBytes(request);
            DeliverReply(callback, kRegisterReply);
            return score::cpp::expected_blank<score::os::Error>{};
        });
    ASSERT_EQ(client.Connect(std::chrono::seconds{1}), std::optional<std::uint16_t>{0xABCDU});

    std::vector<Bytes> sent;
    EXPECT_CALL(first_.mock, Send(_)).Times(2).WillRepeatedly([&sent](auto message) {
        sent.push_back(ToBytes(message));
        return score::cpp::expected_blank<score::os::Error>{};
    });
    EXPECT_TRUE(client.RequestService(0x1234U, 0x5678U, 0x9AU, 0x01020304U));
    EXPECT_TRUE(client.ReleaseService(0x1234U, 0x5678U));

    constexpr Bytes kExpectedRegister{
        0x01U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    constexpr Bytes kExpectedRequest{
        0x01U, 0x02U, 0x34U, 0x12U, 0x78U, 0x56U, 0x9AU, 0x00U, 0x04U, 0x03U, 0x02U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U};
    constexpr Bytes kExpectedRelease{
        0x01U, 0x03U, 0x34U, 0x12U, 0x78U, 0x56U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    EXPECT_EQ(register_request, kExpectedRegister);
    ASSERT_EQ(sent.size(), 2U);
    EXPECT_EQ(sent[0], kExpectedRequest);
    EXPECT_EQ(sent[1], kExpectedRelease);

    constexpr Bytes kAvailable{
        0x01U, 0x81U, 0x34U, 0x12U, 0x78U, 0x56U, 0x9AU, 0x01U, 0x04U, 0x03U, 0x02U, 0x01U, 0x00U, 0x00U, 0xCDU, 0xABU};
    constexpr Bytes kUnavailable{
        0x01U, 0x81U, 0x21U, 0x43U, 0x65U, 0x87U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    first_.notify_callback(AsSpan(kAvailable));
    first_.notify_callback(AsSpan(kUnavailable));
    EXPECT_EQ(available_services, (std::vector<std::pair<std::uint16_t, std::uint16_t>>{{0x1234U, 0x5678U}}));
    EXPECT_EQ(unavailable_services, (std::vector<std::pair<std::uint16_t, std::uint16_t>>{{0x4321U, 0x8765U}}));
}

// Verifies: tests-5 — the Stop() call returns before the transport reports kStopped; the failed Connect() does not
// return before that later kStopped callback.
TEST_F(SdClientTest, RegistrationTimeoutWaitsForLaterStoppedCallback)
{
    ExpectOneConnection();
    first_.ExpectStartReady();
    std::optional<IClientConnection::ReplyCallback> pending_reply;
    EXPECT_CALL(first_.mock, SendWithCallback(_, _)).WillOnce([&pending_reply](auto, auto callback) {
        pending_reply.emplace(std::move(callback));
        return score::cpp::expected_blank<score::os::Error>{};
    });
    Signal stop_called;
    EXPECT_CALL(first_.mock, Stop()).WillOnce([this, &stop_called]() {
        first_.state_callback(IClientConnection::State::kStopping);
        stop_called.Set();
    });
    EXPECT_CALL(first_.mock, Send(_)).Times(0);
    SdClient client{factory_mock_, {}, {}};

    Signal connect_returned;
    std::optional<std::uint16_t> connect_result{0U};
    std::thread connect_thread{[&client, &connect_result, &connect_returned]() {
        connect_result = client.Connect(std::chrono::milliseconds{0});
        connect_returned.Set();
    }};
    const bool saw_stop = stop_called.WaitFor();
    const bool returned_before_stopped = connect_returned.IsSet();
    // The controlled, later kStopped callback from a different thread than the one calling Stop().
    first_.state_callback(IClientConnection::State::kStopped);
    connect_thread.join();

    EXPECT_TRUE(saw_stop);
    EXPECT_FALSE(returned_before_stopped);
    EXPECT_FALSE(connect_result.has_value());
    EXPECT_FALSE(client.RequestService(1U, 2U, 3U, 4U));
}

// Verifies: tests-5 — a late registration reply owns only its completion state and cannot affect a later session.
TEST_F(SdClientTest, LateRegistrationReplyCannotAffectSubsequentSession)
{
    ExpectTwoConnections();
    first_.ExpectStartReady();
    first_.ExpectSynchronousStop();
    std::optional<IClientConnection::ReplyCallback> late_reply;
    EXPECT_CALL(first_.mock, SendWithCallback(_, _)).WillOnce([&late_reply](auto, auto callback) {
        late_reply.emplace(std::move(callback));
        return score::cpp::expected_blank<score::os::Error>{};
    });
    EXPECT_CALL(first_.mock, Send(_)).Times(0);
    constexpr std::uint16_t kSecondClientId{0x2222U};
    second_.ExpectReadySession(kSecondClientId);
    EXPECT_CALL(second_.mock, Send(_)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
    SdClient client{factory_mock_, {}, {}};

    EXPECT_FALSE(client.Connect(std::chrono::milliseconds{0}).has_value());
    ASSERT_EQ(client.Connect(std::chrono::seconds{1}), std::optional<std::uint16_t>{kSecondClientId});

    ASSERT_TRUE(late_reply.has_value());
    DeliverReply(late_reply.value(), RegisterReply(0x1111U));

    EXPECT_EQ(client.Connect(std::chrono::seconds{1}), std::optional<std::uint16_t>{kSecondClientId});
    EXPECT_TRUE(client.RequestService(1U, 2U, 3U, 4U));
}

// Verifies: tests-5, tests-16, tests-21 — destruction stops the connection and does not return before the transport
// reports kStopped asynchronously from another thread, after Stop() itself has already returned.
TEST_F(SdClientTest, DestructorWaitsForAsynchronousLaterStoppedCallback)
{
    ExpectOneConnection();
    first_.ExpectStartReady();
    first_.ExpectRegistration(0x1U);
    Signal stop_called;
    EXPECT_CALL(first_.mock, Stop()).WillOnce([this, &stop_called]() {
        first_.state_callback(IClientConnection::State::kStopping);
        stop_called.Set();
    });
    EXPECT_CALL(first_.mock, Send(_)).Times(0);
    auto client = std::make_unique<SdClient>(
        factory_mock_, SdClient::AvailabilityHandler{}, SdClient::SubscriptionStateHandler{});
    ASSERT_TRUE(client->Connect(std::chrono::seconds{1}).has_value());

    Signal destructor_returned;
    std::thread teardown_thread{[&client, &destructor_returned]() {
        client.reset();
        destructor_returned.Set();
    }};
    const bool saw_stop = stop_called.WaitFor();
    // kStopped has not been reported yet, so the destructor must still be waiting.
    const bool returned_before_stopped = destructor_returned.IsSet();
    first_.state_callback(IClientConnection::State::kStopped);
    const bool returned_after_stopped = destructor_returned.WaitFor();
    teardown_thread.join();

    EXPECT_TRUE(saw_stop);
    EXPECT_FALSE(returned_before_stopped);
    EXPECT_TRUE(returned_after_stopped);
    EXPECT_EQ(client, nullptr);
}

// Verifies: tests-20 — a transport that never leaves kStarting makes Connect() time out, the connection is stopped
// exactly once, nothing is sent on it, and a later Connect() establishes a working session.
TEST_F(SdClientTest, ConnectTimesOutWhileStartingStopsOnceAndLaterConnectWorks)
{
    ExpectTwoConnections();
    EXPECT_CALL(first_.mock, Start(_, _)).WillOnce([this](auto state_cb, auto notify_cb) {
        first_.state_callback = std::move(state_cb);
        first_.notify_callback = std::move(notify_cb);
        first_.state_callback(IClientConnection::State::kStarting);
    });
    first_.ExpectSynchronousStop();
    EXPECT_CALL(first_.mock, SendWithCallback(_, _)).Times(0);
    EXPECT_CALL(first_.mock, Send(_)).Times(0);
    constexpr std::uint16_t kClientId{0x88U};
    second_.ExpectReadySession(kClientId);
    EXPECT_CALL(second_.mock, Send(_)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
    SdClient client{factory_mock_, {}, {}};

    EXPECT_FALSE(client.Connect(std::chrono::milliseconds{10}).has_value());
    EXPECT_FALSE(client.RequestService(1U, 2U, 3U, 4U));

    EXPECT_EQ(client.Connect(std::chrono::seconds{1}), std::optional<std::uint16_t>{kClientId});
    EXPECT_TRUE(client.RequestService(1U, 2U, 3U, 4U));
}

// Verifies: tests-20 — once the transport reports kStopping on a registered session, sends fail without reaching the
// transport.
TEST_F(SdClientTest, SendAfterStoppingFailsWithoutReachingTransport)
{
    ExpectOneConnection();
    first_.ExpectReadySession(0x1U);
    EXPECT_CALL(first_.mock, Send(_)).Times(0);
    SdClient client{factory_mock_, {}, {}};
    ASSERT_TRUE(client.Connect(std::chrono::seconds{1}).has_value());

    first_.state_callback(IClientConnection::State::kStopping);

    EXPECT_FALSE(client.RequestService(1U, 2U, 3U, 4U));
}

TEST_F(SdClientTest, DeliversDecodedNotifications)
{
    ExpectOneConnection();
    first_.ExpectReadySession(0x1U);
    bool availability_notified{false};
    bool subscription_notified{false};
    SdClient client{factory_mock_,
                    [&availability_notified](std::uint16_t service, std::uint16_t instance, bool available) {
                        availability_notified = (service == 5U) && (instance == 6U) && available;
                    },
                    [&subscription_notified](
                        std::uint16_t service, std::uint16_t instance, std::uint16_t eventgroup, bool acknowledged) {
                        subscription_notified =
                            (service == 5U) && (instance == 6U) && (eventgroup == 7U) && acknowledged;
                    }};

    ASSERT_TRUE(client.Connect(std::chrono::seconds{1}).has_value());
    protocol::Message notification{};
    notification.opcode = protocol::Opcode::kAvailability;
    notification.service = 5U;
    notification.instance = 6U;
    notification.flag = 1U;
    first_.notify_callback(AsSpan(protocol::Encode(notification)));
    EXPECT_TRUE(availability_notified);

    notification.opcode = protocol::Opcode::kSubscriptionState;
    notification.eventgroup = 7U;
    first_.notify_callback(AsSpan(protocol::Encode(notification)));
    EXPECT_TRUE(subscription_notified);
}

class SdClientMalformedFlagTest : public SdClientTest, public ::testing::WithParamInterface<std::uint8_t>
{
};

// Verifies: security-9 — availability/subscription-state messages with a flag other than 0 or 1 are dropped.
TEST_P(SdClientMalformedFlagTest, DropsStateNotificationsWithMalformedFlag)
{
    ExpectOneConnection();
    first_.ExpectReadySession(0x1U);
    std::vector<bool> availability_flags;
    std::vector<bool> subscription_flags;
    SdClient client{factory_mock_,
                    [&availability_flags](std::uint16_t, std::uint16_t, bool available) {
                        availability_flags.push_back(available);
                    },
                    [&subscription_flags](std::uint16_t, std::uint16_t, std::uint16_t, bool acknowledged) {
                        subscription_flags.push_back(acknowledged);
                    }};
    ASSERT_TRUE(client.Connect(std::chrono::seconds{1}).has_value());

    protocol::Message notification{};
    notification.service = 5U;
    notification.instance = 6U;
    notification.eventgroup = 7U;
    notification.flag = GetParam();
    notification.opcode = protocol::Opcode::kAvailability;
    first_.notify_callback(AsSpan(protocol::Encode(notification)));
    notification.opcode = protocol::Opcode::kSubscriptionState;
    first_.notify_callback(AsSpan(protocol::Encode(notification)));
    EXPECT_TRUE(availability_flags.empty());
    EXPECT_TRUE(subscription_flags.empty());

    // Boundary: the valid flag 0 is still delivered.
    notification.flag = 0U;
    notification.opcode = protocol::Opcode::kAvailability;
    first_.notify_callback(AsSpan(protocol::Encode(notification)));
    notification.opcode = protocol::Opcode::kSubscriptionState;
    first_.notify_callback(AsSpan(protocol::Encode(notification)));
    EXPECT_EQ(availability_flags, (std::vector<bool>{false}));
    EXPECT_EQ(subscription_flags, (std::vector<bool>{false}));
}

INSTANTIATE_TEST_SUITE_P(MalformedFlags,
                         SdClientMalformedFlagTest,
                         ::testing::Values(std::uint8_t{2U}, std::uint8_t{255U}));

enum class RegistrationFailure : std::uint8_t
{
    kSendFails,
    kReplyError,
    kMalformedReply,
    kWrongOpcodeReply,
    kStoppedBeforeReady,
};

class SdClientRegistrationFailureTest : public SdClientTest, public ::testing::WithParamInterface<RegistrationFailure>
{
};

// Verifies: tests-15 — every registration error path cleans up the connection, sends nothing further and a later
// Connect() establishes a new, working session.
TEST_P(SdClientRegistrationFailureTest, FailedRegistrationCleansUpAndAllowsLaterConnect)
{
    const RegistrationFailure failure = GetParam();
    ExpectTwoConnections();
    if (failure == RegistrationFailure::kStoppedBeforeReady)
    {
        EXPECT_CALL(first_.mock, Start(_, _)).WillOnce([this](auto state_cb, auto notify_cb) {
            first_.state_callback = std::move(state_cb);
            first_.notify_callback = std::move(notify_cb);
            first_.state_callback(IClientConnection::State::kStarting);
            first_.state_callback(IClientConnection::State::kStopping);
            first_.state_callback(IClientConnection::State::kStopped);
        });
        EXPECT_CALL(first_.mock, SendWithCallback(_, _)).Times(0);
        EXPECT_CALL(first_.mock, Stop()).Times(0);
    }
    else
    {
        first_.ExpectStartReady();
        first_.ExpectSynchronousStop();
        EXPECT_CALL(first_.mock, SendWithCallback(_, _))
            .WillOnce([failure](auto, auto callback) -> score::cpp::expected_blank<score::os::Error> {
                protocol::Message reply{};
                reply.opcode = protocol::Opcode::kRegister;
                reply.client_id = 0x55U;
                auto reply_buffer = protocol::Encode(reply);
                switch (failure)
                {
                    case RegistrationFailure::kSendFails:
                        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN));
                    case RegistrationFailure::kReplyError:
                        callback(Reply{score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))});
                        return {};
                    case RegistrationFailure::kMalformedReply:
                        reply_buffer[0] = static_cast<std::uint8_t>(protocol::kVersion + 1U);
                        DeliverReply(callback, reply_buffer);
                        return {};
                    case RegistrationFailure::kWrongOpcodeReply:
                        reply_buffer[1] = static_cast<std::uint8_t>(protocol::Opcode::kRequestService);
                        DeliverReply(callback, reply_buffer);
                        return {};
                    case RegistrationFailure::kStoppedBeforeReady:
                    default:
                        ADD_FAILURE() << "unexpected registration failure kind";
                        return {};
                }
            });
    }
    EXPECT_CALL(first_.mock, Send(_)).Times(0);
    constexpr std::uint16_t kClientId{0x77U};
    second_.ExpectReadySession(kClientId);
    EXPECT_CALL(second_.mock, Send(_)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
    SdClient client{factory_mock_, {}, {}};

    EXPECT_FALSE(client.Connect(std::chrono::seconds{1}).has_value());
    EXPECT_FALSE(client.RequestService(1U, 2U, 3U, 4U));

    EXPECT_EQ(client.Connect(std::chrono::seconds{1}), std::optional<std::uint16_t>{kClientId});
    EXPECT_TRUE(client.RequestService(1U, 2U, 3U, 4U));
}

INSTANTIATE_TEST_SUITE_P(RegistrationErrorPaths,
                         SdClientRegistrationFailureTest,
                         ::testing::Values(RegistrationFailure::kSendFails,
                                           RegistrationFailure::kReplyError,
                                           RegistrationFailure::kMalformedReply,
                                           RegistrationFailure::kWrongOpcodeReply,
                                           RegistrationFailure::kStoppedBeforeReady));

struct DestructionSync
{
    Signal owner_locked;
    Signal handler_entered;
    Signal destructor_entered;
    Signal notification_done;
    bool deadlock_detected{false};
};

/// Connection whose destructor, like the real transport, waits until a running callback has returned.
class CallbackAwareConnection final : public IClientConnection
{
  public:
    explicit CallbackAwareConnection(DestructionSync& sync) : IClientConnection{}, sync_{sync} {}
    ~CallbackAwareConnection() override
    {
        sync_.destructor_entered.Set();
        if (!sync_.notification_done.WaitFor())
        {
            sync_.deadlock_detected = true;
        }
    }
    CallbackAwareConnection(const CallbackAwareConnection&) = delete;
    CallbackAwareConnection(CallbackAwareConnection&&) = delete;
    CallbackAwareConnection& operator=(const CallbackAwareConnection&) = delete;
    CallbackAwareConnection& operator=(CallbackAwareConnection&&) = delete;

    score::cpp::expected_blank<score::os::Error> Send(score::cpp::span<const std::uint8_t>) noexcept override
    {
        return {};
    }
    score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> SendWaitReply(
        score::cpp::span<const std::uint8_t>,
        score::cpp::span<std::uint8_t>) noexcept override
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOTSUP));
    }
    score::cpp::expected_blank<score::os::Error> SendWithCallback(score::cpp::span<const std::uint8_t>,
                                                                  ReplyCallback callback) noexcept override
    {
        DeliverReply(callback, RegisterReply(0x1U));
        return {};
    }
    State GetState() const noexcept override
    {
        return State::kReady;
    }
    StopReason GetStopReason() const noexcept override
    {
        return StopReason::kNone;
    }
    void Start(StateCallback state_callback, NotifyCallback notify_callback) noexcept override
    {
        state_callback_ = std::move(state_callback);
        notify_callback_ = std::move(notify_callback);
        state_callback_(State::kStarting);
        state_callback_(State::kReady);
    }
    void Stop() noexcept override
    {
        state_callback_(State::kStopping);
        state_callback_(State::kStopped);
    }
    void Restart() noexcept override {}

    void PeerClosed()
    {
        state_callback_(State::kStopping);
    }
    void DeliverNotification(const Bytes& buffer)
    {
        notify_callback_(AsSpan(buffer));
        sync_.notification_done.Set();
    }

  private:
    DestructionSync& sync_;
    StateCallback state_callback_;
    NotifyCallback notify_callback_;
};

// Verifies: cpp-5 — the connection is destroyed without holding the connection lock. Ordering reproduced with
// controlled threads: a notification callback blocks on an owner lock, the owner thread holds that lock and sends,
// while Connect() destroys the old connection whose destructor waits for the running callback.
TEST_F(SdClientTest, ConnectionIsDestroyedWithoutHoldingTheConnectionLock)
{
    DestructionSync sync;
    CallbackAwareConnection* connection{nullptr};
    EXPECT_CALL(factory_mock_, Create(_, IsNonBlockingClientConfig()))
        .WillOnce([&sync, &connection](auto&, auto&) {
            auto created =
                score::cpp::pmr::make_unique<CallbackAwareConnection>(score::cpp::pmr::new_delete_resource(), sync);
            connection = created.get();
            return score::cpp::pmr::unique_ptr<IClientConnection>{std::move(created)};
        })
        .WillOnce([](auto&, auto&) {
            return score::cpp::pmr::unique_ptr<IClientConnection>{};
        });
    std::mutex owner_mutex;
    SdClient client{factory_mock_,
                    [&sync, &owner_mutex](std::uint16_t, std::uint16_t, bool) {
                        sync.handler_entered.Set();
                        const std::lock_guard owner_lock{owner_mutex};
                    },
                    {}};
    ASSERT_TRUE(client.Connect(std::chrono::seconds{1}).has_value());
    ASSERT_NE(connection, nullptr);

    bool request_result{true};
    std::thread owner_thread{[&sync, &owner_mutex, &client, &request_result]() {
        const std::lock_guard owner_lock{owner_mutex};
        sync.owner_locked.Set();
        if (sync.destructor_entered.WaitFor())
        {
            request_result = client.RequestService(1U, 2U, 3U, 4U);
        }
    }};
    const bool owner_locked = sync.owner_locked.WaitFor();

    protocol::Message notification{};
    notification.opcode = protocol::Opcode::kAvailability;
    notification.service = 1U;
    notification.instance = 2U;
    notification.flag = 1U;
    const auto notification_buffer = protocol::Encode(notification);
    std::thread ipc_thread{[connection, &notification_buffer]() {
        connection->DeliverNotification(notification_buffer);
    }};
    const bool handler_entered = sync.handler_entered.WaitFor();

    connection->PeerClosed();
    const auto reconnect_result = client.Connect(std::chrono::seconds{1});
    owner_thread.join();
    ipc_thread.join();

    EXPECT_TRUE(owner_locked);
    EXPECT_TRUE(handler_entered);
    EXPECT_FALSE(sync.deadlock_detected);
    EXPECT_FALSE(request_result);
    EXPECT_FALSE(reconnect_result.has_value());
}

}  // namespace
}  // namespace score::someip_sd
