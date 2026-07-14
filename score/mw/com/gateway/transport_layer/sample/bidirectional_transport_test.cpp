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
#include "score/mw/com/gateway/transport_layer/sample/bidirectional_transport.h"
#include "score/mw/com/gateway/transport_layer/sample/mock_message_framer.h"
#include "score/mw/com/gateway/transport_layer/sample/mock_pending_request_tracker.h"
#include "score/mw/com/gateway/transport_layer/sample/sample_transport_test_resources.h"
#include "score/os/mocklib/socketmock.h"
#include "score/os/socket.h"
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <memory>
#include <thread>

namespace score::mw::com::gateway
{
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace
{
MessageHeader CreateMessageHeader(MessageType type, std::uint32_t sequence = 0U, std::uint32_t payload_size = 0U)
{
    MessageHeader header{};
    header.type = type;
    header.sequence = sequence;
    header.payload_size = payload_size;
    return header;
}

}  // namespace

/// \brief Test fixture for BidirectionalTransport with mocked dependencies.
class BidirectionalTransportWithMocksFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        auto framer = std::make_unique<MockMessageFramer>();
        auto tracker = std::make_unique<MockPendingRequestTracker>();
        framer_mock_ = framer.get();
        tracker_mock_ = tracker.get();
        auto config = HyperVisorSocketConfiguration{score::os::Ipv4Address{"127.0.0.1"}};
        config.request_timeout_ms_ = 50U;
        transport_ = std::make_shared<BidirectionalTransport>(std::move(config), std::move(framer), std::move(tracker));
        attorney_ = std::make_unique<BidirectionalTransportAttorney>(transport_);
    }
    void TearDown() override
    {
        if (transport_ != nullptr)
        {
            EXPECT_CALL(*tracker_mock_, NotifyAll()).Times(::testing::AnyNumber());
            transport_->Shutdown();
        }
    }
    BidirectionalTransportWithMocksFixture& WithConnectedTransport()
    {
        attorney_->SetIsConnected(true);
        return *this;
    }
    BidirectionalTransportWithMocksFixture& WithFullRequestLifecycle(std::uint32_t sequence = 1U)
    {
        EXPECT_CALL(*tracker_mock_, GetNextSequenceNumber()).WillOnce(Return(sequence));
        EXPECT_CALL(*tracker_mock_, RegisterPendingRequest(sequence));
        EXPECT_CALL(*tracker_mock_, ErasePendingRequest(sequence));
        return *this;
    }
    BidirectionalTransportWithMocksFixture& WithSendMessageSucceeding()
    {
        EXPECT_CALL(*framer_mock_, SendMessage(_, _)).WillOnce(Return(score::ResultBlank{}));
        return *this;
    }
    BidirectionalTransportWithMocksFixture& WithWaitForAckSucceeding(std::uint32_t sequence = 1U)
    {
        EXPECT_CALL(*tracker_mock_, WaitForAck(sequence, _, _)).WillOnce(Return(score::ResultBlank{}));
        return *this;
    }

    MockMessageFramer* framer_mock_{nullptr};
    MockPendingRequestTracker* tracker_mock_{nullptr};
    std::shared_ptr<BidirectionalTransport> transport_;
    std::unique_ptr<BidirectionalTransportAttorney> attorney_;
};
TEST_F(BidirectionalTransportWithMocksFixture, SendNotificationReturnsNotConnectedWhenDisconnected)
{
    // Given a transport that is not connected
    StopOfferServiceRequest message{};

    // When SendNotification is called
    const auto result = transport_->SendNotification(message);

    // Then the call fails with kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}
TEST_F(BidirectionalTransportWithMocksFixture, SendNotificationSucceedsWhenConnected)
{
    // Given a connected transport where SendMessage succeeds
    WithConnectedTransport().WithSendMessageSucceeding();

    StopOfferServiceRequest message{};

    // When SendNotification is called
    const auto result = transport_->SendNotification(message);

    // Then the call succeeds
    EXPECT_TRUE(result.has_value());
}
TEST_F(BidirectionalTransportWithMocksFixture, SendNotificationFailsWhenSendMessageFails)
{
    // Given a connected transport where SendMessage returns an error
    WithConnectedTransport();
    EXPECT_CALL(*framer_mock_, SendMessage(_, _))
        .WillOnce(Return(score::MakeUnexpected(TransportErrorc::kSendFailure)));

    StopOfferServiceRequest message{};

    // When SendNotification is called
    const auto result = transport_->SendNotification(message);

    // Then the call fails with kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}
TEST_F(BidirectionalTransportWithMocksFixture, SendRequestReturnsNotConnectedWhenDisconnected)
{
    // Given a transport that is not connected
    StopOfferServiceRequest message{};

    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then the call fails with kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}
TEST_F(BidirectionalTransportWithMocksFixture, SendRequestSucceedsWhenAckArrives)
{
    // Given a connected transport where send succeeds and an ACK arrives
    std::uint32_t sequence_number = 1U;
    WithConnectedTransport()
        .WithFullRequestLifecycle(sequence_number)
        .WithSendMessageSucceeding()
        .WithWaitForAckSucceeding(1U);

    StopOfferServiceRequest message{};

    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then the request completes successfully
    EXPECT_TRUE(result.has_value());
}
TEST_F(BidirectionalTransportWithMocksFixture, SendRequestTimeoutReturnsTimeoutError)
{
    // Given a connected transport where ACKs will never be received due to timeouts
    WithConnectedTransport().WithFullRequestLifecycle(1U);
    EXPECT_CALL(*framer_mock_, SendMessage(_, _)).WillRepeatedly(Return(score::ResultBlank{}));
    EXPECT_CALL(*tracker_mock_, WaitForAck(1U, _, _))
        .WillRepeatedly(Return(score::MakeUnexpected(TransportErrorc::kTimeout)));
    EXPECT_CALL(*tracker_mock_, ResetAcknowledgement(1U)).Times(::testing::AnyNumber());

    StopOfferServiceRequest message{};

    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then it fails with kTimeout
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kTimeout);
}
TEST_F(BidirectionalTransportWithMocksFixture, SendRequestRetriesOnTimeout)
{
    // Given a connected transport where for the given attempts ACKs will never be received due to timeouts
    WithConnectedTransport().WithFullRequestLifecycle(1U);
    EXPECT_CALL(*framer_mock_, SendMessage(_, _))
        .Times(::testing::Exactly(5))
        .WillRepeatedly(Return(score::ResultBlank{}));
    EXPECT_CALL(*tracker_mock_, WaitForAck(1U, _, _))
        .Times(::testing::Exactly(5))
        .WillRepeatedly(Return(score::MakeUnexpected(TransportErrorc::kTimeout)));
    EXPECT_CALL(*tracker_mock_, ResetAcknowledgement(1U)).Times(4);

    StopOfferServiceRequest message{};

    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then it retries sending the message for kMaxSendRetries times and fails with kTimeout
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kTimeout);
}
TEST_F(BidirectionalTransportWithMocksFixture, SendRequestReturnsNotConnectedIfDisconnectedDuringWaitForAck)
{
    // Given a connected transport where during waiting for ACKs the connection state switches to 'disconnected'
    WithConnectedTransport().WithFullRequestLifecycle(1U).WithSendMessageSucceeding();
    EXPECT_CALL(*tracker_mock_, WaitForAck(1U, _, _))
        .WillOnce(
            Invoke([this](std::uint32_t, std::chrono::milliseconds, const std::atomic<bool>&) -> score::ResultBlank {
                attorney_->SetIsConnected(false);
                return score::MakeUnexpected(TransportErrorc::kNotConnected);
            }));

    StopOfferServiceRequest message{};

    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then it fails with kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}
TEST_F(BidirectionalTransportWithMocksFixture, SendRequestReturnsNotConnectedIfDisconnectedBeforeSending)
{
    // Given a connected transport that switches to state 'disconnected' during request registration
    std::uint32_t sequence_number = 1U;
    WithConnectedTransport();
    EXPECT_CALL(*tracker_mock_, GetNextSequenceNumber()).WillOnce(Return(sequence_number));
    EXPECT_CALL(*tracker_mock_, RegisterPendingRequest(sequence_number)).WillOnce(Invoke([this](std::uint32_t) {
        attorney_->SetIsConnected(false);
    }));
    EXPECT_CALL(*tracker_mock_, ErasePendingRequest(sequence_number));

    StopOfferServiceRequest message{};

    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then the request fails with kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}
TEST_F(BidirectionalTransportWithMocksFixture, SendRequestFailsWhenSendMessageFails)
{
    // Given a connected transport where SendMessage fails and causes a state switch to 'disconnected'
    WithConnectedTransport().WithFullRequestLifecycle(1U);
    EXPECT_CALL(*framer_mock_, SendMessage(_, _))
        .WillOnce(Invoke([this](std::int32_t, const TransportMessage&) -> score::ResultBlank {
            attorney_->SetIsConnected(false);
            return score::MakeUnexpected(TransportErrorc::kSendFailure);
        }));

    StopOfferServiceRequest message{};

    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then the request fails with kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}
TEST_F(BidirectionalTransportWithMocksFixture, SendRequestReturnsNotConnectedIfDisconnectedDuringRetry)
{
    // Given a connected transport where receiving the ACK times out and a disconnect occurs simultaneously
    std::uint32_t sequence_number = 1U;
    WithConnectedTransport().WithFullRequestLifecycle(sequence_number).WithSendMessageSucceeding();
    EXPECT_CALL(*tracker_mock_, WaitForAck(sequence_number, _, _))
        .WillOnce(
            Invoke([this](std::uint32_t, std::chrono::milliseconds, const std::atomic<bool>&) -> score::ResultBlank {
                attorney_->SetIsConnected(false);
                return score::MakeUnexpected(TransportErrorc::kTimeout);
            }));

    StopOfferServiceRequest message{};

    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then it does not retry (WaitForAck only called once) and fails with kTimeout since disconnect was detected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kTimeout);
}
TEST_F(BidirectionalTransportWithMocksFixture, IsConnectedReturnsFalseAfterConstruction)
{
    // Given a freshly constructed transport
    // When IsConnected is queried
    // Then it reports false
    EXPECT_FALSE(transport_->IsConnected());
}
TEST_F(BidirectionalTransportWithMocksFixture, IsConnectedReturnsTrueWhenConnected)
{
    // Given a transport marked as connected
    WithConnectedTransport();

    // When IsConnected is queried
    // Then it reports true
    EXPECT_TRUE(transport_->IsConnected());
}
TEST_F(BidirectionalTransportWithMocksFixture, MessageHandlerCanBeRegistered)
{
    // Given a transport instance

    // When a message handler is registered
    transport_->SetMessageHandler([](std::unique_ptr<TransportMessage>) {});

    // Then the message handler is set
    EXPECT_TRUE(attorney_->IsMessageHandlerSet());
}
TEST_F(BidirectionalTransportWithMocksFixture, ShutdownStopsTransport)
{
    // Given a transport in state 'connected'
    WithConnectedTransport();
    EXPECT_CALL(*tracker_mock_, NotifyAll());

    // When Shutdown is called
    transport_->Shutdown();

    // Then the transport is in state 'disconnected'
    EXPECT_FALSE(transport_->IsConnected());
}
TEST_F(BidirectionalTransportWithMocksFixture, MultipleShutdownsAreIdempotent)
{
    // Given a transport in state 'connected'
    WithConnectedTransport();
    EXPECT_CALL(*tracker_mock_, NotifyAll()).Times(::testing::AtLeast(1));

    // When Shutdown is called multiple times
    transport_->Shutdown();
    EXPECT_FALSE(transport_->IsConnected());

    transport_->Shutdown();

    // Then the transport remains in state 'disconnected'
    EXPECT_FALSE(transport_->IsConnected());
}
/// \brief Socket-based fixture: tests that are requiring actual socket mocking for setup, receive and send calls.
class BidirectionalTransportSocketFixture : public ::testing::Test
{
  public:
    static constexpr std::int32_t kListenFd = 100;
    static constexpr std::int32_t kSendFd = 101;
    static constexpr std::int32_t kReceiveFd = 102;

    void SetUp() override
    {
        score::os::Socket::set_testing_instance(socket_mock_);
    }
    void TearDown() override
    {
        if (transport_ != nullptr)
        {
            transport_->Shutdown();
            transport_.reset();
        }
        score::os::Socket::restore_instance();
    }
    BidirectionalTransportSocketFixture& CreateTransport(std::uint32_t timeout_ms = 50U)
    {
        auto config = HyperVisorSocketConfiguration{score::os::Ipv4Address{"127.0.0.1"}};
        config.request_timeout_ms_ = timeout_ms;
        transport_ = std::make_shared<BidirectionalTransport>(std::move(config));
        return *this;
    }
    BidirectionalTransportSocketFixture& SocketWillBeCreatedAndSockOptionsSet()
    {
        EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM | SOCK_NONBLOCK, 0))
            .WillOnce(Return(kListenFd));
        EXPECT_CALL(socket_mock_, setsockopt(_, _, _, _, _))
            .WillRepeatedly(Return(score::cpp::expected_blank<score::os::Error>{}));
        return *this;
    }
    BidirectionalTransportSocketFixture& WithASocketBoundAndListening()
    {
        SocketWillBeCreatedAndSockOptionsSet();
        EXPECT_CALL(socket_mock_, bind(_, _, _)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
        EXPECT_CALL(socket_mock_, listen(_, _)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
        return *this;
    }

    BidirectionalTransportSocketFixture& WithAConnectedSendSocket()
    {
        EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM, 0)).WillOnce(Return(kSendFd));
        EXPECT_CALL(socket_mock_, connect(kSendFd, _, _))
            .WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
        return *this;
    }

    BidirectionalTransportSocketFixture& WithASocketSetupThatIsConnectedAndHasAccepted()
    {
        WithASocketBoundAndListening().WithAConnectedSendSocket();
        EXPECT_CALL(socket_mock_, accept(kListenFd, _, _)).WillOnce(Return(kReceiveFd));
        return *this;
    }
    BidirectionalTransportSocketFixture& WithSendWillWriteAllBytes()
    {
        EXPECT_CALL(socket_mock_, sendto(kSendFd, _, _, _, _, _))
            .WillRepeatedly(Invoke([](auto, auto, const std::size_t len, auto, auto, auto)
                                       -> score::cpp::expected<ssize_t, score::os::Error> {
                return static_cast<ssize_t>(len);
            }));
        return *this;
    }

    BidirectionalTransportSocketFixture& WithAcceptReturningErrorOnFirstCall(std::uint8_t error_code)
    {
        ::testing::InSequence seq;
        EXPECT_CALL(socket_mock_, accept(kListenFd, _, _))
            .WillOnce(Return(score::cpp::expected<std::int32_t, score::os::Error>{
                score::cpp::make_unexpected(score::os::Error::createFromErrno(error_code))}));
        EXPECT_CALL(socket_mock_, accept(kListenFd, _, _)).WillOnce(Return(kReceiveFd));
        return *this;
    }

    score::os::SocketMock socket_mock_;
    std::shared_ptr<BidirectionalTransport> transport_;
};
TEST_F(BidirectionalTransportSocketFixture, SetupFailsWhenListenSocketCreationFails)
{
    // Given a transport for which no listening socket can be created
    CreateTransport();
    EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM | SOCK_NONBLOCK, 0))
        .WillOnce(Return(score::cpp::expected<std::int32_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EACCES))}));

    // When Setup is called
    const auto result = transport_->Setup();

    // Then Setup returns kConnectionFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kConnectionFailure);
}
TEST_F(BidirectionalTransportSocketFixture, SetupFailsWhenBindFails)
{
    // Given a transport with a listening socket but for which bind() fails
    CreateTransport().SocketWillBeCreatedAndSockOptionsSet();
    EXPECT_CALL(socket_mock_, bind(_, _, _))
        .WillOnce(Return(score::cpp::expected_blank<score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EADDRINUSE))}));

    // When Setup is called
    const auto result = transport_->Setup();

    // Then Setup returns kConnectionFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kConnectionFailure);
}
TEST_F(BidirectionalTransportSocketFixture, SetupFailsWhenListenSocketCreationFailsWithDifferentError)
{
    // Given a transport where creating the listening socket fails with ENOMEM
    CreateTransport();
    EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM | SOCK_NONBLOCK, 0))
        .WillOnce(Return(score::cpp::expected<std::int32_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM))}));

    // When Setup is called
    const auto result = transport_->Setup();

    // Then Setup returns kConnectionFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kConnectionFailure);
}
TEST_F(BidirectionalTransportSocketFixture, SetupFailsWhenListenFails)
{
    // Given a transport where calling listen on the listening socket fails
    CreateTransport().SocketWillBeCreatedAndSockOptionsSet();
    EXPECT_CALL(socket_mock_, bind(_, _, _)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
    EXPECT_CALL(socket_mock_, listen(_, _))
        .WillOnce(Return(score::cpp::expected_blank<score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EADDRINUSE))}));

    // When Setup is called
    const auto result = transport_->Setup();

    // Then Setup returns kConnectionFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kConnectionFailure);
}
TEST_F(BidirectionalTransportSocketFixture, SetupFailsWhenSendSocketCreationFails)
{
    // Given a transport where the listen socket setup succeeds but creating the send socket fails
    CreateTransport().WithASocketBoundAndListening();
    EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM, 0))
        .WillOnce(Return(score::cpp::expected<std::int32_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EMFILE))}));
    EXPECT_CALL(socket_mock_, accept(kListenFd, _, _))
        .WillRepeatedly(Return(score::cpp::expected<std::int32_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN))}));

    score::ResultBlank result = {};
    std::thread setup_thread([this, &result]() {
        result = transport_->Setup();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // When Shutdown() is called after the send socket creation has failed
    // Shutdown() needs to be called in order to leave Setup()
    transport_->Shutdown();
    setup_thread.join();

    // Then Setup returns kConnectionFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kConnectionFailure);
    EXPECT_FALSE(transport_->IsConnected());
}
TEST_F(BidirectionalTransportSocketFixture, SetupFailsWhenSendSocketRecreationFailsAfterConnectFailure)
{
    // Given a transport where the listen socket setup succeeds, the initial send socket is created but connect fails,
    // and then recreating the socket for retry also fails
    CreateTransport().WithASocketBoundAndListening();
    EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM, 0))
        .WillOnce(Return(kSendFd))
        .WillOnce(Return(score::cpp::expected<std::int32_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EMFILE))}));
    EXPECT_CALL(socket_mock_, connect(kSendFd, _, _))
        .WillOnce(Return(score::cpp::expected_blank<score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(ECONNREFUSED))}));
    EXPECT_CALL(socket_mock_, accept(kListenFd, _, _))
        .WillRepeatedly(Return(score::cpp::expected<std::int32_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN))}));

    score::ResultBlank result = {};
    std::thread setup_thread([this, &result]() {
        result = transport_->Setup();
    });

    // Wait longer than the 50ms retry interval in SetupSendSocket so the socket recreation is attempted
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // When Shutdown() is called and therefore Setup() is left
    transport_->Shutdown();
    setup_thread.join();

    // Then Setup returns kConnectionFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kConnectionFailure);
    EXPECT_FALSE(transport_->IsConnected());
}

TEST_F(BidirectionalTransportSocketFixture, SetupReturnsConnectionFailureWhenShutdownInterruptsConnectionAttempt)
{
    // Given Setup() retries connection with connect/accept returning errors and therefore blocking Setup()
    CreateTransport(50U).WithASocketBoundAndListening();
    EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM, 0)).WillRepeatedly(Return(kSendFd));
    EXPECT_CALL(socket_mock_, connect(_, _, _))
        .WillRepeatedly(Return(score::cpp::expected_blank<score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(ECONNREFUSED))}));
    EXPECT_CALL(socket_mock_, accept(_, _, _))
        .WillRepeatedly(Return(score::cpp::expected<std::int32_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN))}));

    score::ResultBlank result = {};
    std::thread setup_thread([this, &result]() {
        result = transport_->Setup();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // When Shutdown() is called while Setup is retrying
    transport_->Shutdown();
    setup_thread.join();

    // Then Setup returns kConnectionFailure and transport is disconnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kConnectionFailure);
    EXPECT_FALSE(transport_->IsConnected());
}

TEST_F(BidirectionalTransportSocketFixture, SetupRetriesAcceptAfterEagain)
{
    // Given a transport where accept() on the listening socket returns EAGAIN on the first call and succeeds on the
    // second
    CreateTransport().WithASocketBoundAndListening().WithAConnectedSendSocket().WithAcceptReturningErrorOnFirstCall(
        EAGAIN);

    // Block recv to keep connection alive
    std::shared_ptr<std::atomic<bool>> allow_disconnect{new std::atomic<bool>{false}};
    EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
        .WillOnce(Invoke([allow_disconnect](
                             auto, void*, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
            while (!allow_disconnect->load() && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return static_cast<ssize_t>(0);
        }));

    // When Setup is called
    const auto setup_result = transport_->Setup();

    // Then Setup succeeds (accept retried after EAGAIN and eventually succeeded)
    EXPECT_TRUE(setup_result.has_value());
    EXPECT_TRUE(transport_->IsConnected());

    allow_disconnect->store(true, std::memory_order_relaxed);
    transport_->Shutdown();
    transport_.reset();
}

TEST_F(BidirectionalTransportSocketFixture, MissingMessageHandlerCallbackIsAllowed)
{
    // Given a transport with connected sockets and no message handler set
    CreateTransport().WithASocketSetupThatIsConnectedAndHasAccepted();
    BidirectionalTransportAttorney attorney{transport_};

    MessageHeader inbound_header = CreateMessageHeader(MessageType::kStopOfferServiceRequest, 42U, 0U);

    std::shared_ptr<std::atomic<bool>> allow_disconnect{new std::atomic<bool>{false}};
    {
        ::testing::InSequence seq;
        // First recv returns the size of a valid message header
        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
            .WillOnce(Invoke([&inbound_header](auto, void* buffer, const std::size_t, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                inbound_header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }));
        // Block recv to keep the connection alive until clean up
        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
            .WillOnce(
                Invoke([allow_disconnect](
                           auto, void*, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                    while (!allow_disconnect->load() && std::chrono::steady_clock::now() < deadline)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    return static_cast<ssize_t>(0);
                }));
    }

    // When Setup is called which sets up the receiving socket to receive the incoming request
    const auto setup_result = transport_->Setup();
    ASSERT_TRUE(setup_result.has_value());
    ASSERT_TRUE(attorney.WaitForConnectedState(true, std::chrono::milliseconds(200)));

    // Wait for the message to be processed (received and discarded)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Then the transport is still connected and valid
    EXPECT_TRUE(transport_->IsConnected());

    allow_disconnect->store(true, std::memory_order_relaxed);
    transport_->Shutdown();
    transport_.reset();
}

TEST_F(BidirectionalTransportSocketFixture, ConstructorAndDestructorWork)
{
    // Given a valid socket configuration
    HyperVisorSocketConfiguration config{score::os::Ipv4Address{"127.0.0.1"}};
    config.local_port_ = 9000;
    config.remote_port_ = 9001;

    // When a transport object is constructed
    BidirectionalTransport transport(config);

    // Then construction succeeds and initial state is disconnected
    EXPECT_FALSE(transport.IsConnected());
}

TEST_F(BidirectionalTransportSocketFixture, IncomingRequestIsReceivedAndDispatched)
{
    // Given connected sockets with an incoming request message that will be received
    constexpr std::uint32_t kIncomingSequence = 77U;

    CreateTransport().WithASocketSetupThatIsConnectedAndHasAccepted().WithSendWillWriteAllBytes();

    MessageHeader inbound_header = CreateMessageHeader(MessageType::kStopOfferServiceRequest, kIncomingSequence, 0U);

    std::shared_ptr<std::atomic<bool>> allow_disconnect{new std::atomic<bool>{false}};
    EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
        .WillOnce(
            Invoke([&inbound_header](
                       auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                inbound_header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }))
        .WillOnce(Invoke([allow_disconnect](
                             auto, void*, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
            while (!allow_disconnect->load() && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return static_cast<ssize_t>(0);
        }));

    std::atomic<bool> handler_called{false};
    std::atomic<std::uint32_t> received_sequence{0U};
    transport_->SetMessageHandler([&handler_called, &received_sequence](std::unique_ptr<TransportMessage> message) {
        if (message != nullptr)
        {
            received_sequence = message->GetSequenceNumber();
            handler_called = true;
        }
    });

    // When Setup is called and therefore the receiving socket is setup to receive the incoming request
    const auto setup_result = transport_->Setup();
    ASSERT_TRUE(setup_result.has_value());

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (!handler_called.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Then the request is dispatched to the handler and the message has the correct sequence number
    EXPECT_TRUE(handler_called.load());
    EXPECT_EQ(received_sequence.load(), kIncomingSequence);

    allow_disconnect->store(true, std::memory_order_relaxed);
    transport_->Shutdown();
    transport_.reset();
}
TEST_F(BidirectionalTransportSocketFixture, ReceivesIncomingAckResponseAndCompletesPendingRequest)
{
    // Given a connected transport with a pending SendRequest waiting for an ACK
    constexpr std::uint32_t kRequestSequence = 1U;

    CreateTransport(40U).WithASocketSetupThatIsConnectedAndHasAccepted().WithSendWillWriteAllBytes();
    BidirectionalTransportAttorney attorney{transport_};

    MessageHeader ack_header = CreateMessageHeader(MessageType::kAckResponse, 999U, sizeof(std::uint32_t));

    std::atomic<bool> allow_reading_ack_header{false};
    std::shared_ptr<std::atomic<bool>> allow_disconnect{new std::atomic<bool>{false}};

    {
        ::testing::InSequence in_sequence;
        // Block receiving until message has been sent and pending request is registered
        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
            .WillOnce(Invoke([&allow_reading_ack_header, &ack_header](auto, void* buffer, const std::size_t, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                while (!allow_reading_ack_header.load() && std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                ack_header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }));

        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, sizeof(std::uint32_t), _))
            .WillOnce(Invoke(
                [](auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                    const std::uint32_t acked_sequence = kRequestSequence;
                    std::memcpy(buffer, &acked_sequence, sizeof(acked_sequence));
                    return static_cast<ssize_t>(sizeof(acked_sequence));
                }));

        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
            .WillOnce(
                Invoke([allow_disconnect](
                           auto, void*, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                    while (!allow_disconnect->load() && std::chrono::steady_clock::now() < deadline)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    return static_cast<ssize_t>(0);
                }));
    }

    const auto setup_result = transport_->Setup();
    ASSERT_TRUE(setup_result.has_value());

    // When sending a request
    StopOfferServiceRequest message{};
    score::ResultBlank result = score::MakeUnexpected(TransportErrorc::kSendFailure);
    std::atomic<bool> send_request_finished{false};
    std::thread send_request_thread([this, &message, &result, &send_request_finished]() {
        result = transport_->SendRequest(message);
        send_request_finished = true;
    });

    // State that there is a pending request and that receiving messages is enabled
    ASSERT_TRUE(attorney.WaitForPendingRequest(kRequestSequence, std::chrono::milliseconds(200)));
    allow_reading_ack_header = true;

    const auto request_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (!send_request_finished.load() && std::chrono::steady_clock::now() < request_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!send_request_finished.load())
    {
        allow_disconnect->store(true, std::memory_order_relaxed);
        transport_->Shutdown();
    }

    send_request_thread.join();

    // Then the pending request is completed successfully
    EXPECT_TRUE(result.has_value());

    allow_disconnect->store(true, std::memory_order_relaxed);
    transport_->Shutdown();
    transport_.reset();
}
TEST_F(BidirectionalTransportWithMocksFixture, DisconnectsWhenReceiveMessageReturnsNullptr)
{
    // Given a connected transport where ReceiveMessage returns nullptr (i.e any read error ocurred)
    WithConnectedTransport();
    EXPECT_CALL(*framer_mock_, ReceiveMessage(_)).WillOnce(Return(::testing::ByMove(nullptr)));

    // When the receive loop processes the nullptr result
    attorney_->RunReceiveUntilDisconnect();

    // Then the transport disconnects
    EXPECT_FALSE(transport_->IsConnected());
}
TEST_F(BidirectionalTransportWithMocksFixture, ReceiveLoopProcessesValidMessageAndDisconnectsOnNullptr)
{
    // Given a connected transport that receives a valid request message followed by an invalid one
    WithConnectedTransport();
    constexpr std::uint32_t kSequence = 77U;
    auto valid_message = std::make_unique<StopOfferServiceRequest>();
    valid_message->SetSequenceNumber(kSequence);

    // SendAck will be called synchronously for the incoming request
    EXPECT_CALL(*framer_mock_, SendMessage(_, _)).WillOnce(Return(score::ResultBlank{}));

    {
        ::testing::InSequence seq;
        EXPECT_CALL(*framer_mock_, ReceiveMessage(_)).WillOnce(Return(::testing::ByMove(std::move(valid_message))));
        EXPECT_CALL(*framer_mock_, ReceiveMessage(_)).WillOnce(Return(::testing::ByMove(nullptr)));
    }

    transport_->SetMessageHandler([](std::unique_ptr<TransportMessage>) {});

    // When the receive loop runs
    attorney_->RunReceiveUntilDisconnect();

    // Then the valid message was processed (SendAck verified via EXPECT_CALL) and the transport disconnects on nullptr.
    // Note: the handler itself is not called here because the DispatchLoop thread (which drains the dispatch queue and
    // invokes the handler) is not running — it is only started by Setup(). Handler delivery is tested separately in
    // DispatchThreadDeliversQueuedMessageToHandler.
    EXPECT_FALSE(transport_->IsConnected());
}
}  // namespace score::mw::com::gateway
