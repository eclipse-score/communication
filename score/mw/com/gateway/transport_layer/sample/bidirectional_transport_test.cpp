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
#include "score/mw/com/gateway/transport_layer/sample/sample_transport_test_resources.h"

#include "score/os/mocklib/socketmock.h"
#include "score/os/socket.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <thread>

namespace score::mw::com::gateway
{

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace
{

// Test message that intentionally fails to serialize and deserialize
class ZeroSerializeMessage : public TransportMessage
{
  public:
    ZeroSerializeMessage() : TransportMessage(MessageType::kStopOfferServiceRequest) {}
    std::size_t Serialize(score::cpp::span<std::uint8_t>) const override
    {
        return 0U;
    }
    bool Deserialize(score::cpp::span<const std::uint8_t>) override
    {
        return false;
    }
};

}  // namespace

class BidirectionalTransportFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        score::os::Socket::set_testing_instance(socket_mock_);
    }

    void TearDown() override
    {
        if (transport_ != nullptr)
        {
            transport_->Shutdown();
        }
        score::os::Socket::restore_instance();
    }

    BidirectionalTransportFixture& CreateTransport(std::uint32_t timeout_ms = 50U)
    {
        auto config = HyperVisorSocketConfiguration{score::os::Ipv4Address{"127.0.0.1"}};
        config.request_timeout_ms_ = timeout_ms;
        transport_ = std::make_shared<BidirectionalTransport>(std::move(config));
        return *this;
    }

    BidirectionalTransportFixture& SendWillWriteAllBytes(std::int32_t fd)
    {
        EXPECT_CALL(socket_mock_, sendto(fd, _, _, _, _, _))
            .WillRepeatedly(Invoke([](auto, auto, const std::size_t len, auto, auto, auto)
                                       -> score::cpp::expected<ssize_t, score::os::Error> {
                return static_cast<ssize_t>(len);
            }));
        return *this;
    }

    BidirectionalTransportFixture& SendWillNeverBeCalled(std::int32_t fd)
    {
        EXPECT_CALL(socket_mock_, sendto(fd, _, _, _, _, _)).Times(0);
        return *this;
    }

    BidirectionalTransportFixture& SendWillReturnError(std::int32_t fd, int errno_val)
    {
        EXPECT_CALL(socket_mock_, sendto(fd, _, _, _, _, _))
            .WillOnce(Return(score::cpp::expected<ssize_t, score::os::Error>{
                score::cpp::make_unexpected(score::os::Error::createFromErrno(errno_val))}));
        return *this;
    }

    BidirectionalTransportFixture& SendWillWriteAllBytesAndCountCalls(std::int32_t fd, std::atomic<int>& sendto_count)
    {
        EXPECT_CALL(socket_mock_, sendto(fd, _, _, _, _, _))
            .WillRepeatedly(Invoke([&sendto_count](auto, auto, const std::size_t len, auto, auto, auto)
                                       -> score::cpp::expected<ssize_t, score::os::Error> {
                sendto_count++;
                return static_cast<ssize_t>(len);
            }));
        return *this;
    }

    BidirectionalTransportFixture& SendWillSucceedAndThenFail(std::int32_t fd, int errno_val)
    {
        EXPECT_CALL(socket_mock_, sendto(fd, _, _, _, _, _))
            .WillOnce(Invoke([](auto, auto, const std::size_t len, auto, auto, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                return static_cast<ssize_t>(len);
            }))
            .WillOnce(Return(score::cpp::expected<ssize_t, score::os::Error>{
                score::cpp::make_unexpected(score::os::Error::createFromErrno(errno_val))}));
        return *this;
    }

    BidirectionalTransportFixture& SendWillFailAndChangeStateToDisconnected(std::int32_t fd,
                                                                            BidirectionalTransportAttorney& attorney)
    {
        EXPECT_CALL(socket_mock_, sendto(fd, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke([&attorney](auto, auto, const std::size_t, auto, auto, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                attorney.SetIsConnected(false);
                return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
            }));
        return *this;
    }

    BidirectionalTransportFixture& SocketWillBeCreatedAndSockOptionsSet(std::int32_t listen_fd)
    {
        EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM | SOCK_NONBLOCK, 0))
            .WillOnce(Return(listen_fd));
        EXPECT_CALL(socket_mock_, setsockopt(_, _, _, _, _))
            .WillRepeatedly(Return(score::cpp::expected_blank<score::os::Error>{}));
        return *this;
    }

    BidirectionalTransportFixture& WithADefaultSocketSetup(std::int32_t listen_fd)
    {
        SocketWillBeCreatedAndSockOptionsSet(listen_fd);
        EXPECT_CALL(socket_mock_, bind(_, _, _)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
        EXPECT_CALL(socket_mock_, listen(_, _)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
        return *this;
    }

    BidirectionalTransportFixture& WithADefaultSocketSetupInStableMode(std::int32_t listen_fd, std::int32_t send_fd)
    {
        // If the socket is already connected, the connection loop should not disrupt that state, so by returning errors
        // for connect() and accept() on other calls no connection will be established
        WithADefaultSocketSetup(listen_fd);
        EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM, 0))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(Return(send_fd));
        EXPECT_CALL(socket_mock_, connect(send_fd, _, _))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(Return(score::cpp::expected_blank<score::os::Error>{
                score::cpp::make_unexpected(score::os::Error::createFromErrno(ECONNREFUSED))}));
        EXPECT_CALL(socket_mock_, accept(listen_fd, _, _))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(Return(score::cpp::expected<std::int32_t, score::os::Error>{
                score::cpp::make_unexpected(score::os::Error::createFromErrno(EAGAIN))}));
        return *this;
    }

    BidirectionalTransportFixture& WithASocketSetupThatIsConnectedAndHasAccepted(std::int32_t listen_fd,
                                                                                 std::int32_t send_fd,
                                                                                 std::int32_t receive_fd)
    {
        WithADefaultSocketSetup(listen_fd);
        EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM, 0)).WillOnce(Return(send_fd));
        EXPECT_CALL(socket_mock_, connect(send_fd, _, _))
            .WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
        EXPECT_CALL(socket_mock_, accept(listen_fd, _, _)).WillOnce(Return(receive_fd));
        return *this;
    }

    score::os::SocketMock socket_mock_;
    std::shared_ptr<BidirectionalTransport> transport_;
};

TEST_F(BidirectionalTransportFixture, MessageHandlerCanBeRegistered)
{
    // Given a transport instance
    CreateTransport();
    BidirectionalTransportAttorney attorney{transport_};

    // When a message handler is registered
    transport_->SetMessageHandler([](std::unique_ptr<TransportMessage>) {});

    // Then the message handler is set
    EXPECT_TRUE(attorney.IsMessageHandlerSet());
}

TEST_F(BidirectionalTransportFixture, SendNotificationReturnsNotConnectedWhenDisconnected)
{
    // Given a transport that is not connected.
    CreateTransport();

    StopOfferServiceRequest message{};
    // When SendNotification is called
    const auto result = transport_->SendNotification(message);

    // Then the call fails with kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}

TEST_F(BidirectionalTransportFixture, SendNotificationFailsWhenSerializeReturnsZero)
{
    // Given a connected transport and a message that serializes to zero bytes
    CreateTransport();
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(2001);

    ZeroSerializeMessage message{};
    // When SendNotification is called with a message that has a 0 byte payload
    const auto result = transport_->SendNotification(message);

    // Then the call fails with kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}

TEST_F(BidirectionalTransportFixture, SendNotificationSucceedsWhenConnected)
{
    // Given a connected transport and send() is enabled for this socket
    const int socket_number = 2002;
    CreateTransport().SendWillWriteAllBytes(socket_number);
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(socket_number);

    StopOfferServiceRequest message{};
    // When SendNotification is called
    const auto result = transport_->SendNotification(message);

    // Then the call succeeds
    EXPECT_TRUE(result.has_value());
}

TEST_F(BidirectionalTransportFixture, SendRequestSucceedsWhenAckArrives)
{
    // Given a connected transport where send() is enabled for this socket and a pending request gets acknowledged
    const uint32_t timeout = 40U;
    const int socket_number = 3001;
    const uint32_t sequence_to_ack = 1U;
    CreateTransport(timeout).SendWillWriteAllBytes(socket_number);
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(socket_number);

    // Acknowledge the pending request in a separate thread before SendRequest times out
    std::thread ack_thread([&attorney]() {
        if (attorney.WaitForPendingRequest(sequence_to_ack, std::chrono::milliseconds(100)))
        {
            attorney.AcknowledgePendingRequest(sequence_to_ack);
        }
    });

    StopOfferServiceRequest message{};
    // When SendRequest is called and the ACK is set before the timeout expires
    const auto result = transport_->SendRequest(message);
    ack_thread.join();

    // Then the request completes successfully
    EXPECT_TRUE(result.has_value());
}

TEST_F(BidirectionalTransportFixture, SendRequestReturnsNotConnectedIfDisconnectedBeforeSending)
{
    const uint32_t timeout = 20U;
    const int socket_number = 3002;
    // Given transport in initial state 'connected'
    CreateTransport(timeout).SendWillNeverBeCalled(socket_number);
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(socket_number);

    attorney.LockPendingMutex();

    // When switching the state to disconnected before triggering SendRequest() in a separate thread
    score::ResultBlank result = score::MakeUnexpected(TransportErrorc::kSendFailure);
    std::thread request_thread([this, &result]() {
        StopOfferServiceRequest request{};
        // When SendRequest resumes
        result = transport_->SendRequest(request);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    attorney.SetIsConnected(false);
    attorney.UnlockPendingMutex();

    request_thread.join();

    // Then sending the request fails with kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}

TEST_F(BidirectionalTransportFixture, SetupFailsWhenListenSocketCreationFails)
{
    // Given a transport for which no listening socket can be created
    CreateTransport();

    EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM | SOCK_NONBLOCK, 0))
        .WillOnce(Return(score::cpp::expected<std::int32_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EACCES))}));

    // When Setup() is called
    const auto result = transport_->Setup();

    // Then Setup() returns kConnectionFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kConnectionFailure);
}

TEST_F(BidirectionalTransportFixture, SetupFailsWhenBindFails)
{
    // Given a transport with a listening socket but for which bind() can not be called
    CreateTransport().SocketWillBeCreatedAndSockOptionsSet(4001);

    EXPECT_CALL(socket_mock_, bind(_, _, _))
        .WillOnce(Return(score::cpp::expected_blank<score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EADDRINUSE))}));

    // When Setup is called
    const auto result = transport_->Setup();

    // Then Setup returns kConnectionFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kConnectionFailure);
}

TEST_F(BidirectionalTransportFixture, DispatchThreadDeliversQueuedMessageToHandler)
{
    // Given connected sockets and receive blocks until the test releases it
    constexpr std::int32_t kListenFd = 6001;
    constexpr std::int32_t kSendFd = 6002;
    constexpr std::int32_t kReceiveFd = 6003;
    CreateTransport().WithASocketSetupThatIsConnectedAndHasAccepted(kListenFd, kSendFd, kReceiveFd);
    BidirectionalTransportAttorney attorney{transport_};

    std::atomic<bool> allow_disconnect_read{false};
    EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
        .WillOnce(Invoke([&allow_disconnect_read](
                             auto, void*, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
            while (!allow_disconnect_read.load() && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return static_cast<ssize_t>(0);
        }));

    std::promise<std::uint32_t> delivered_sequence_promise;
    auto delivered_sequence_future = delivered_sequence_promise.get_future();
    std::atomic<bool> handler_reported{false};
    transport_->SetMessageHandler(
        [&handler_reported, &delivered_sequence_promise](std::unique_ptr<TransportMessage> message) {
            if (message != nullptr && message->GetType() == MessageType::kStopOfferServiceRequest &&
                !handler_reported.exchange(true))
            {
                delivered_sequence_promise.set_value(message->GetSequenceNumber());
            }
        });

    // When a message is queued for dispatch
    const auto setup_result = transport_->Setup();
    ASSERT_TRUE(setup_result.has_value());
    ASSERT_TRUE(attorney.WaitForConnectedState(true, std::chrono::milliseconds(200)));

    auto queued = std::make_unique<StopOfferServiceRequest>();
    queued->SetSequenceNumber(42U);
    attorney.EnqueueForDispatch(std::move(queued));

    // Then the dispatch thread invokes the handler
    EXPECT_EQ(delivered_sequence_future.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
    if (delivered_sequence_future.valid())
    {
        EXPECT_EQ(delivered_sequence_future.get(), 42U);
    }

    allow_disconnect_read = true;
    transport_->Shutdown();
}

TEST_F(BidirectionalTransportFixture, SetupReceivesIncomingRequestDispatchesItAndSendsAck)
{
    // Given connected sockets
    constexpr std::int32_t kListenFd = 6101;
    constexpr std::int32_t kSendFd = 6102;
    constexpr std::int32_t kReceiveFd = 6103;
    constexpr std::uint32_t kIncomingSequence = 77U;

    CreateTransport()
        .WithASocketSetupThatIsConnectedAndHasAccepted(kListenFd, kSendFd, kReceiveFd)
        .SendWillWriteAllBytes(kSendFd);

    MessageHeader inbound_header{};
    inbound_header.type = MessageType::kStopOfferServiceRequest;
    inbound_header.sequence = kIncomingSequence;
    inbound_header.payload_size = 0U;

    EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
        .WillOnce(
            Invoke([&inbound_header](
                       auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                inbound_header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }))
        .WillOnce(
            Invoke([this](auto, void*, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
                while (transport_->IsConnected() && std::chrono::steady_clock::now() < deadline)
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

    // When Setup starts receive processing.
    const auto setup_result = transport_->Setup();
    ASSERT_TRUE(setup_result.has_value());

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (!handler_called.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Then the request is dispatched and processed with an ACK send.
    EXPECT_TRUE(handler_called.load());
    EXPECT_EQ(received_sequence.load(), kIncomingSequence);

    transport_->Shutdown();
}

TEST_F(BidirectionalTransportFixture, ReceivesIncomingAckResponseAndCompletesPendingRequest)
{
    // Given setup receive path returns an ACK payload for a pending request.
    constexpr std::int32_t kListenFd = 6201;
    constexpr std::int32_t kSendFd = 6202;
    constexpr std::int32_t kReceiveFd = 6203;
    constexpr std::uint32_t kRequestSequence = 1U;

    CreateTransport(40U)
        .WithASocketSetupThatIsConnectedAndHasAccepted(kListenFd, kSendFd, kReceiveFd)
        .SendWillWriteAllBytes(kSendFd);
    BidirectionalTransportAttorney attorney{transport_};

    MessageHeader ack_header{};
    ack_header.type = MessageType::kAckResponse;
    ack_header.sequence = 999U;
    ack_header.payload_size = sizeof(std::uint32_t);
    std::atomic<bool> allow_ack_header_read{false};
    std::atomic<bool> allow_disconnect_read{false};

    {
        ::testing::InSequence in_sequence;

        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
            .WillOnce(Invoke([&allow_ack_header_read, &ack_header](auto, void* buffer, const std::size_t, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                while (!allow_ack_header_read.load() && std::chrono::steady_clock::now() < deadline)
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
                Invoke([&allow_disconnect_read](
                           auto, void*, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                    while (!allow_disconnect_read.load() && std::chrono::steady_clock::now() < deadline)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    return static_cast<ssize_t>(0);
                }));
    }

    const auto setup_result = transport_->Setup();
    ASSERT_TRUE(setup_result.has_value());

    StopOfferServiceRequest message{};
    score::ResultBlank result = score::MakeUnexpected(TransportErrorc::kSendFailure);
    std::atomic<bool> request_finished{false};
    std::thread request_thread([this, &message, &result, &request_finished]() {
        // When SendRequest is called after Setup.
        result = transport_->SendRequest(message);
        request_finished = true;
    });

    ASSERT_TRUE(attorney.WaitForPendingRequest(kRequestSequence, std::chrono::milliseconds(200)));
    allow_ack_header_read = true;

    const auto request_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (!request_finished.load() && std::chrono::steady_clock::now() < request_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!request_finished.load())
    {
        allow_disconnect_read = true;
        transport_->Shutdown();
    }

    request_thread.join();

    // Then the pending request is completed successfully
    EXPECT_TRUE(result.has_value());

    allow_disconnect_read = true;
    transport_->Shutdown();
}

TEST_F(BidirectionalTransportFixture, SetupDisconnectsOnOversizedIncomingPayload)
{
    // Given Setup reads an incoming header with an oversized payload.
    constexpr std::int32_t kListenFd = 6301;
    constexpr std::int32_t kSendFd = 6302;
    constexpr std::int32_t kReceiveFd = 6303;

    CreateTransport().WithASocketSetupThatIsConnectedAndHasAccepted(kListenFd, kSendFd, kReceiveFd);
    BidirectionalTransportAttorney attorney{transport_};

    MessageHeader inbound_header{};
    inbound_header.type = MessageType::kAckResponse;
    inbound_header.sequence = 11U;
    inbound_header.payload_size = 1025U;
    std::atomic<bool> allow_header_read{false};

    // Then Setup fails and the transport disconnects.
    EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
        .WillOnce(
            Invoke([&inbound_header, &allow_header_read](
                       auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                while (!allow_header_read.load() && std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                inbound_header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }));

    score::ResultBlank setup_result = score::MakeUnexpected(TransportErrorc::kConnectionFailure);
    std::atomic<bool> setup_finished{false};
    std::thread setup_thread([this, &setup_result, &setup_finished]() {
        // When the header is processed.
        setup_result = transport_->Setup();
        setup_finished = true;
    });

    const bool connected_seen = attorney.WaitForConnectedState(true, std::chrono::milliseconds(200));
    allow_header_read = true;

    if (!connected_seen)
    {
        transport_->Shutdown();
        setup_thread.join();
        FAIL() << "Setup() did not reach connected state before oversized payload";
    }

    const auto setup_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (!setup_finished.load() && std::chrono::steady_clock::now() < setup_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!setup_finished.load())
    {
        transport_->Shutdown();
    }

    setup_thread.join();

    ASSERT_FALSE(setup_result.has_value());
    EXPECT_EQ(setup_result.error(), TransportErrorc::kConnectionFailure);
    EXPECT_TRUE(attorney.WaitForConnectedState(false, std::chrono::milliseconds(100)));

    transport_->Shutdown();
}

TEST_F(BidirectionalTransportFixture, SetupDisconnectsWhenIncomingPayloadReadFails)
{
    // Given Setup reads a valid header but payload read returns EOF.
    constexpr std::int32_t kListenFd = 6401;
    constexpr std::int32_t kSendFd = 6402;
    constexpr std::int32_t kReceiveFd = 6403;

    CreateTransport().WithASocketSetupThatIsConnectedAndHasAccepted(kListenFd, kSendFd, kReceiveFd);
    BidirectionalTransportAttorney attorney{transport_};

    MessageHeader inbound_header{};
    inbound_header.type = MessageType::kAckResponse;
    inbound_header.sequence = 12U;
    inbound_header.payload_size = sizeof(std::uint32_t);

    std::atomic<bool> allow_header_read{false};

    {
        ::testing::InSequence in_sequence;

        // First receive call returns size of header bytes — gated so the disconnect cannot
        // race with Setup() observing is_connected_ == true.
        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
            .WillOnce(Invoke([&inbound_header, &allow_header_read](auto, void* buffer, const std::size_t, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                while (!allow_header_read.load() && std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                inbound_header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }));
        // Follow-up receive call, to read payload, returns 0 bytes immediately
        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, sizeof(std::uint32_t), _))
            .WillOnce(
                Invoke([](auto, void*, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                    return score::cpp::expected<ssize_t, score::os::Error>{0};
                }));
    }

    score::ResultBlank setup_result = score::MakeUnexpected(TransportErrorc::kConnectionFailure);
    std::atomic<bool> setup_finished{false};
    std::thread setup_thread([this, &setup_result, &setup_finished]() {
        // When receive processing continues
        setup_result = transport_->Setup();
        setup_finished = true;
    });

    const bool connected_seen = attorney.WaitForConnectedState(true, std::chrono::milliseconds(200));
    allow_header_read = true;

    if (!connected_seen)
    {
        transport_->Shutdown();
        setup_thread.join();
        FAIL() << "Setup() did not reach connected state before payload read failure";
    }

    const auto setup_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (!setup_finished.load() && std::chrono::steady_clock::now() < setup_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!setup_finished.load())
    {
        transport_->Shutdown();
    }

    setup_thread.join();

    // Then Setup fails and the transport disconnects
    ASSERT_FALSE(setup_result.has_value());
    EXPECT_EQ(setup_result.error(), TransportErrorc::kConnectionFailure);
    EXPECT_TRUE(attorney.WaitForConnectedState(false, std::chrono::milliseconds(100)));

    transport_->Shutdown();
}

TEST_F(BidirectionalTransportFixture, SetupDisconnectsOnUnknownIncomingMessageType)
{
    // Given Setup reads an incoming header with an invalid message type
    constexpr std::int32_t kListenFd = 6501;
    constexpr std::int32_t kSendFd = 6502;
    constexpr std::int32_t kReceiveFd = 6503;

    CreateTransport().WithASocketSetupThatIsConnectedAndHasAccepted(kListenFd, kSendFd, kReceiveFd);
    BidirectionalTransportAttorney attorney{transport_};

    MessageHeader inbound_header{};
    inbound_header.type = MessageType::kInvalid;
    inbound_header.sequence = 13U;
    inbound_header.payload_size = 0U;
    std::atomic<bool> allow_header_read{false};

    EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
        .WillOnce(
            Invoke([&inbound_header, &allow_header_read](
                       auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                while (!allow_header_read.load() && std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                inbound_header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }));

    score::ResultBlank setup_result = score::MakeUnexpected(TransportErrorc::kConnectionFailure);
    std::atomic<bool> setup_finished{false};
    std::thread setup_thread([this, &setup_result, &setup_finished]() {
        // When the header is handled.
        setup_result = transport_->Setup();
        setup_finished = true;
    });

    const bool connected_seen = attorney.WaitForConnectedState(true, std::chrono::milliseconds(200));
    allow_header_read = true;

    if (!connected_seen)
    {
        transport_->Shutdown();
        setup_thread.join();
        FAIL() << "Setup() did not reach connected state before unknown message type";
    }

    const auto setup_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (!setup_finished.load() && std::chrono::steady_clock::now() < setup_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!setup_finished.load())
    {
        transport_->Shutdown();
    }

    setup_thread.join();

    // Then Setup fails and the transport disconnects
    ASSERT_FALSE(setup_result.has_value());
    EXPECT_EQ(setup_result.error(), TransportErrorc::kConnectionFailure);
    EXPECT_TRUE(attorney.WaitForConnectedState(false, std::chrono::milliseconds(100)));

    transport_->Shutdown();
}

TEST_F(BidirectionalTransportFixture, SetupDisconnectsOnIncomingDeserializationFailure)
{
    // Given Setup receives a request payload that cannot be deserialized.
    constexpr std::int32_t kListenFd = 6601;
    constexpr std::int32_t kSendFd = 6602;
    constexpr std::int32_t kReceiveFd = 6603;

    CreateTransport().WithASocketSetupThatIsConnectedAndHasAccepted(kListenFd, kSendFd, kReceiveFd);
    BidirectionalTransportAttorney attorney{transport_};

    MessageHeader inbound_header{};
    inbound_header.type = MessageType::kStopOfferServiceRequest;
    inbound_header.sequence = 14U;
    inbound_header.payload_size = 1U;
    std::atomic<bool> allow_payload_read{false};

    {
        ::testing::InSequence in_sequence;

        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, MessageHeader::kWireSize, _))
            .WillOnce(Invoke([&inbound_header](auto, void* buffer, const std::size_t, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                inbound_header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }));
        EXPECT_CALL(socket_mock_, recv(kReceiveFd, _, 1U, _))
            .WillOnce(Invoke([&allow_payload_read](auto, void* buffer, const std::size_t, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                while (!allow_payload_read.load() && std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                static_cast<std::uint8_t*>(buffer)[0] = 0U;
                return static_cast<ssize_t>(1);
            }));
    }

    score::ResultBlank setup_result = score::MakeUnexpected(TransportErrorc::kConnectionFailure);
    std::atomic<bool> setup_finished{false};
    std::thread setup_thread([this, &setup_result, &setup_finished]() {
        // When receive processing handles that message.
        setup_result = transport_->Setup();
        setup_finished = true;
    });

    const bool connected_seen = attorney.WaitForConnectedState(true, std::chrono::milliseconds(200));
    allow_payload_read = true;

    if (!connected_seen)
    {
        transport_->Shutdown();
        setup_thread.join();
        FAIL() << "Setup() did not reach connected state before deserialization failure";
    }

    const auto setup_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (!setup_finished.load() && std::chrono::steady_clock::now() < setup_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!setup_finished.load())
    {
        transport_->Shutdown();
    }

    setup_thread.join();

    // Then Setup fails and the transport disconnects
    ASSERT_FALSE(setup_result.has_value());
    EXPECT_EQ(setup_result.error(), TransportErrorc::kConnectionFailure);
    EXPECT_TRUE(attorney.WaitForConnectedState(false, std::chrono::milliseconds(100)));

    transport_->Shutdown();
}

TEST_F(BidirectionalTransportFixture, IsConnectedReturnsFalseAfterConstruction)
{
    // Given a freshly constructed transport
    CreateTransport();
    // When IsConnected is queried
    // Then it reports false
    EXPECT_FALSE(transport_->IsConnected());
}

TEST_F(BidirectionalTransportFixture, IsConnectedReturnsTrueWhenConnected)
{
    // Given a transport marked as connected
    CreateTransport();
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    // When IsConnected is queried
    // Then it reports true
    EXPECT_TRUE(transport_->IsConnected());
}

TEST_F(BidirectionalTransportFixture, SendRequestReturnsNotConnectedWhenDisconnected)
{
    // Given a transport that is not connected
    CreateTransport();
    StopOfferServiceRequest message{};
    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then the call fails with kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}

TEST_F(BidirectionalTransportFixture, SendRequestReturnsNotConnectedIfDisconnectedDuringRetry)
{
    // Given a connected transport instance where sendto is instrumented to count the number of calls
    std::atomic<int> sendto_count{0};
    CreateTransport(20U).SendWillWriteAllBytesAndCountCalls(3003, sendto_count);
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(3003);

    // When the transport disconnects while trying to send the request
    score::ResultBlank result = score::MakeUnexpected(TransportErrorc::kSendFailure);
    std::thread request_thread([this, &result]() {
        StopOfferServiceRequest request{};
        result = transport_->SendRequest(request);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    attorney.SetIsConnected(false);

    request_thread.join();

    // Then SendRequest fails with kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}

TEST_F(BidirectionalTransportFixture, SendRequestTimeoutReturnsTimeoutError)
{
    const int socket_number = 3004;
    // Given a connected transport that never receives an ACK
    CreateTransport(10U).SendWillWriteAllBytes(socket_number);
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(socket_number);

    StopOfferServiceRequest message{};
    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then it fails with kTimeout
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kTimeout);
}

TEST_F(BidirectionalTransportFixture, SendRequestRetriesOnTimeout)
{
    const int socket_number = 3005;
    // Given a connected transport where sendto is instrumented to count the number of calls and ACKs never arrive.
    std::atomic<int> sendto_count{0};
    CreateTransport(15U).SendWillWriteAllBytesAndCountCalls(socket_number, sendto_count);
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(socket_number);

    // When SendRequest is called
    StopOfferServiceRequest message{};
    const auto result = transport_->SendRequest(message);

    // Then sendto is called multiple times in order to retry
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kTimeout);
    EXPECT_GT(sendto_count, 1);
}

TEST_F(BidirectionalTransportFixture, SendRequestFailsWhenSerializeReturnsZero)
{
    // Given a connected transport
    CreateTransport();
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(3006);

    // When SendRequest is called with a message that serializes to zero bytes.
    ZeroSerializeMessage message{};
    const auto result = transport_->SendRequest(message);

    // Then it fails with kSendFailure.
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}

TEST_F(BidirectionalTransportFixture, ShutdownStopsTransport)
{
    // Given a transport in state 'connected'
    CreateTransport();
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(4001);
    attorney.SetReceiveSocket(4002);
    attorney.SetListenSocket(4003);

    // When Shutdown is called
    transport_->Shutdown();

    // Then the transport is in state 'Disconnected'
    EXPECT_FALSE(transport_->IsConnected());
}

TEST_F(BidirectionalTransportFixture, SetupFailsWhenListenSocketCreationFailsWithDifferentError)
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

TEST_F(BidirectionalTransportFixture, SetupFailsWhenListenFails)
{
    // Given a transport where calling listen on the listening socket fails
    CreateTransport().SocketWillBeCreatedAndSockOptionsSet(5002);

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

TEST_F(BidirectionalTransportFixture, SendNotificationReturnsSendFailureWhenHeaderSendFails)
{
    const int socket_number = 5001;
    // Given a connected transport, where calling sendto on the socket always returns an error
    CreateTransport().SendWillReturnError(socket_number, EPIPE);
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(socket_number);

    StopOfferServiceRequest message{};
    // When SendNotification is called
    const auto result = transport_->SendNotification(message);

    // Then it fails with kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}

TEST_F(BidirectionalTransportFixture, SendNotificationReturnsSendFailureWhenPayloadSendFails)
{
    const int socket_number = 5002;
    // Given a connected transport where payload send fails, after header has been sent successfully
    CreateTransport().SendWillSucceedAndThenFail(socket_number, EPIPE);
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(socket_number);

    // When SendNotification is called
    StopOfferServiceRequest message{};
    const auto result = transport_->SendNotification(message);

    // Then it fails with kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}

TEST_F(BidirectionalTransportFixture, SendRequestDisconnectsDuringWaitForAck)
{
    const int socket_number = 3007;
    // Given SendRequest starts while the transport is connected
    CreateTransport(30U).SendWillWriteAllBytes(socket_number);
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(socket_number);

    std::atomic<bool> send_started{false};
    std::thread disconnect_thread([&attorney, &send_started]() {
        while (!send_started)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        attorney.SetIsConnected(false);
    });

    score::ResultBlank result = score::MakeUnexpected(TransportErrorc::kSendFailure);
    std::thread request_thread([this, &result, &send_started]() {
        send_started = true;
        StopOfferServiceRequest request{};
        // When the transport disconnects during ACK wait.
        result = transport_->SendRequest(request);
    });

    disconnect_thread.join();
    request_thread.join();

    // Then SendRequest fails with kNotConnected
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotConnected);
}

TEST_F(BidirectionalTransportFixture, TrySendAndWaitForAckReturnsErrorWhenSendFails)
{
    const int socket_number = 3008;
    // Given a connected transport where calling sendto on the socket returns an error
    CreateTransport();
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(socket_number);

    SendWillFailAndChangeStateToDisconnected(socket_number, attorney);

    StopOfferServiceRequest message{};
    // When SendRequest is called
    const auto result = transport_->SendRequest(message);

    // Then the request fails with kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}

TEST_F(BidirectionalTransportFixture, SetupReturnsConnectionFailureWhenShutdownInterruptsConnectionAttempt)
{
    // Given during Setup() the calls to create/connect sockets return retryable errors
    CreateTransport(50U).WithADefaultSocketSetup(7001);

    EXPECT_CALL(socket_mock_, socket(score::os::Socket::Domain::kIPv4, SOCK_STREAM, 0)).WillRepeatedly(Return(7002));
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

TEST_F(BidirectionalTransportFixture, ConstructorAndDestructorWork)
{
    // Given a valid socket configuration
    HyperVisorSocketConfiguration config{score::os::Ipv4Address{"127.0.0.1"}};
    config.local_port_ = 9000;
    config.remote_port_ = 9001;

    // When a transport object is constructed and destroyed
    BidirectionalTransport transport(config);
    // Then construction succeeds and initial state is disconnected
    EXPECT_FALSE(transport.IsConnected());
}

TEST_F(BidirectionalTransportFixture, MultipleShutdownsAreIdempotent)
{
    // Given a transport in state connected
    CreateTransport();
    BidirectionalTransportAttorney attorney{transport_};
    attorney.SetIsConnected(true);
    attorney.SetSendSocket(8001);

    // When Shutdown is called multiple times
    transport_->Shutdown();

    EXPECT_FALSE(transport_->IsConnected());
    // Then transport should still be in state 'disconnected'
    transport_->Shutdown();
    EXPECT_FALSE(transport_->IsConnected());
}

}  // namespace score::mw::com::gateway
