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
#include "score/mw/com/gateway/transport_layer/sample/message_framer.h"

#include "score/os/mocklib/socketmock.h"
#include "score/os/socket.h"

#include <gtest/gtest.h>
#include <cstring>

namespace score::mw::com::gateway
{

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace
{

MessageHeader CreateMessageHeader(MessageType type, std::uint32_t sequence, std::size_t payload_size)
{
    MessageHeader header{};
    header.type = type;
    header.sequence = sequence;
    header.payload_size = payload_size;
    return header;
}
}  // namespace

class MessageFramerFixture : public ::testing::Test
{
  protected:
    static constexpr std::int32_t kSocketFd = 42;
    static constexpr std::size_t kHalfHeaderSize = MessageHeader::kWireSize / 2U;
    static constexpr std::size_t kRemainingHeaderSize = MessageHeader::kWireSize - kHalfHeaderSize;

    void SetUp() override
    {
        score::os::Socket::set_testing_instance(socket_mock_);
    }
    void TearDown() override
    {
        score::os::Socket::restore_instance();
    }

    MessageFramerFixture& WithSendWillAllwaysWriteAllBytes()
    {
        EXPECT_CALL(socket_mock_, sendto(kSocketFd, _, _, _, _, _))
            .WillRepeatedly(Invoke([](auto, auto, const std::size_t len, auto, auto, auto)
                                       -> score::cpp::expected<ssize_t, score::os::Error> {
                return static_cast<ssize_t>(len);
            }));
        return *this;
    }

    MessageFramerFixture& WithRecvWillReturnHeaderThenPayload(const MessageHeader& header,
                                                              const void* payload,
                                                              std::size_t payload_size)
    {
        EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
            .WillOnce(Invoke([header](auto, void* buffer, const std::size_t len, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                EXPECT_EQ(len, MessageHeader::kWireSize);
                header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }))
            .WillOnce(Invoke([payload, payload_size](auto, void* buffer, const std::size_t len, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                EXPECT_EQ(len, payload_size);
                std::memcpy(buffer, payload, payload_size);
                return static_cast<ssize_t>(payload_size);
            }));
        return *this;
    }

    score::os::SocketMock socket_mock_;
    MessageFramer framer_;
};

// Tests for static function CreateMessageForType

TEST_F(MessageFramerFixture, CreateMessageForTypeReturnsProvideServiceRequest)
{
    auto msg = MessageFramer::CreateMessageForType(MessageType::kProvideServiceRequest);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(), MessageType::kProvideServiceRequest);
}

TEST_F(MessageFramerFixture, CreateMessageForTypeReturnsOfferServiceRequest)
{
    auto msg = MessageFramer::CreateMessageForType(MessageType::kOfferServiceRequest);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(), MessageType::kOfferServiceRequest);
}

TEST_F(MessageFramerFixture, CreateMessageForTypeReturnsStopOfferServiceRequest)
{
    auto msg = MessageFramer::CreateMessageForType(MessageType::kStopOfferServiceRequest);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(), MessageType::kStopOfferServiceRequest);
}

TEST_F(MessageFramerFixture, CreateMessageForTypeReturnsRegisterNotificationRequest)
{
    auto msg = MessageFramer::CreateMessageForType(MessageType::kRegisterNotificationRequest);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(), MessageType::kRegisterNotificationRequest);
}

TEST_F(MessageFramerFixture, CreateMessageForTypeReturnsUnregisterNotificationRequest)
{
    auto msg = MessageFramer::CreateMessageForType(MessageType::kUnregisterNotificationRequest);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(), MessageType::kUnregisterNotificationRequest);
}

TEST_F(MessageFramerFixture, CreateMessageForTypeReturnsUpdateNotification)
{
    auto msg = MessageFramer::CreateMessageForType(MessageType::kUpdateNotification);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(), MessageType::kUpdateNotification);
}

TEST_F(MessageFramerFixture, CreateMessageForTypeReturnsAckResponse)
{
    auto msg = MessageFramer::CreateMessageForType(MessageType::kAckResponse);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(), MessageType::kAckResponse);
}

TEST_F(MessageFramerFixture, CreateMessageForTypeReturnsNullptrForInvalidType)
{
    auto msg = MessageFramer::CreateMessageForType(MessageType::kInvalid);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageFramerFixture, CreateMessageForTypeReturnsNullptrForUnknownType)
{
    auto msg = MessageFramer::CreateMessageForType(static_cast<MessageType>(999));
    EXPECT_EQ(msg, nullptr);
}

// SendMessage tests

TEST_F(MessageFramerFixture, SendMessageSucceedsForValidMessage)
{
    // Given a valid AckResponse message with data to serialize
    WithSendWillAllwaysWriteAllBytes();
    AckResponse message{55U};
    message.SetSequenceNumber(1U);

    // When SendMessage is called
    const auto result = framer_.SendMessage(kSocketFd, message);

    // Then it succeeds
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessageFramerFixture, SendMessageSucceedsForStopOfferServiceRequest)
{
    // Given a StopOfferServiceRequest message with empty instance specifier and sendto succeeds
    WithSendWillAllwaysWriteAllBytes();
    StopOfferServiceRequest message{};

    // When SendMessage is called
    const auto result = framer_.SendMessage(kSocketFd, message);

    // Then it succeeds
    EXPECT_TRUE(result.has_value());
}

TEST_F(MessageFramerFixture, SendMessageFailsWhenHeaderSendFails)
{
    // Given a valid message but sendto fails on the first call (when sending the header)
    EXPECT_CALL(socket_mock_, sendto(kSocketFd, _, _, _, _, _))
        .WillOnce(Return(score::cpp::expected<ssize_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EIO))}));

    AckResponse message{55U};

    // When SendMessage is called
    const auto result = framer_.SendMessage(kSocketFd, message);

    // Then it fails with kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}

TEST_F(MessageFramerFixture, SendMessageFailsWhenPayloadSendFails)
{
    // Given a valid message where the header send succeeds but the payload send fails
    EXPECT_CALL(socket_mock_, sendto(kSocketFd, _, _, _, _, _))
        .WillOnce(Invoke(
            [](auto, auto, const std::size_t len, auto, auto, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                return static_cast<ssize_t>(len);  // Header succeeds
            }))
        .WillOnce(Return(score::cpp::expected<ssize_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(EIO))}));

    AckResponse message{55U};

    // When SendMessage is called
    const auto result = framer_.SendMessage(kSocketFd, message);

    // Then it fails with kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}

TEST_F(MessageFramerFixture, SendMessageHandlesPartialSend)
{
    // Given a valid message where sendto only writes part of the payload at a time
    std::size_t call_count = 0U;
    EXPECT_CALL(socket_mock_, sendto(kSocketFd, _, _, _, _, _))
        .WillRepeatedly(Invoke([&call_count](auto, auto, const std::size_t len, auto, auto, auto)
                                   -> score::cpp::expected<ssize_t, score::os::Error> {
            call_count++;
            // Return only 1 byte at a time to exercise the loop
            return static_cast<ssize_t>(std::min(len, std::size_t{1}));
        }));

    AckResponse message{55U};

    // When SendMessage is called
    const auto result = framer_.SendMessage(kSocketFd, message);

    // Then it succeeds and sendto() was called multiple times
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(call_count, 2U);
}

TEST_F(MessageFramerFixture, SendMessageFailsForZeroSizePayload)
{
    // Given a message that serializes to zero bytes (invalid)
    class InvalidMessage : public TransportMessage
    {
      public:
        InvalidMessage() : TransportMessage(MessageType::kAckResponse) {}
        std::size_t Serialize(score::cpp::span<std::uint8_t>) const override
        {
            return 0U;
        }
        bool Deserialize(score::cpp::span<const std::uint8_t>) override
        {
            return true;
        }
    };

    InvalidMessage message;

    // When SendMessage is called
    const auto result = framer_.SendMessage(kSocketFd, message);

    // Then it fails with kSendFailure
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kSendFailure);
}

// ReceiveMessage tests

TEST_F(MessageFramerFixture, ReceiveMessageSucceedsForAckResponse)
{
    // Given a socket that returns a valid AckResponse header and payload
    constexpr std::uint32_t kAckedSequence = 123U;
    MessageHeader header = CreateMessageHeader(MessageType::kAckResponse, 7U, sizeof(std::uint32_t));

    std::uint32_t payload = kAckedSequence;
    WithRecvWillReturnHeaderThenPayload(header, &payload, sizeof(payload));

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then a valid AckResponse is returned with correct data
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(message->GetType(), MessageType::kAckResponse);
    EXPECT_EQ(message->GetSequenceNumber(), 7U);
}

TEST_F(MessageFramerFixture, ReceiveMessageReturnsNullptrWhenHeaderRecvFails)
{
    // Given a socket where recv returns an error for the header
    EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
        .WillOnce(Return(score::cpp::expected<ssize_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(ECONNRESET))}));

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then nullptr is returned
    EXPECT_EQ(message, nullptr);
}

TEST_F(MessageFramerFixture, ReceiveMessageReturnsNullptrWhenHeaderRecvReturnsPeerClosed)
{
    // Given a socket where recv returns 0 (peer closed connection)
    EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
        .WillOnce(Return(score::cpp::expected<ssize_t, score::os::Error>{static_cast<ssize_t>(0)}));

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then nullptr is returned
    EXPECT_EQ(message, nullptr);
}

TEST_F(MessageFramerFixture, ReceiveMessageReturnsNullptrForOversizedPayload)
{
    // Given a header with payload_size exceeding kMaxPayloadSize
    MessageHeader header = CreateMessageHeader(MessageType::kAckResponse, 1U, MessageFramer::kMaxPayloadSize + 1U);

    EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
        .WillOnce(Invoke(
            [header](auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }));

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then nullptr is returned
    EXPECT_EQ(message, nullptr);
}

TEST_F(MessageFramerFixture, ReceiveMessageReturnsNullptrWhenPayloadRecvFails)
{
    // Given a valid header but payload recv fails
    MessageHeader header = CreateMessageHeader(MessageType::kAckResponse, 1U, sizeof(std::uint32_t));

    EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
        .WillOnce(Invoke(
            [header](auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }))
        .WillOnce(Return(score::cpp::expected<ssize_t, score::os::Error>{
            score::cpp::make_unexpected(score::os::Error::createFromErrno(ECONNRESET))}));

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then nullptr is returned
    EXPECT_EQ(message, nullptr);
}

TEST_F(MessageFramerFixture, ReceiveMessageReturnsNullptrForUnknownMessageType)
{
    // Given a header with an invalid message type
    MessageHeader header = CreateMessageHeader(MessageType::kInvalid, 1U, 0U);

    EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
        .WillOnce(Invoke(
            [header](auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }));

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then nullptr is returned
    EXPECT_EQ(message, nullptr);
}

TEST_F(MessageFramerFixture, ReceiveMessageReturnsNullptrWhenDeserializationFails)
{
    // Given a valid header for StopOfferServiceRequest but payload contains invalid data
    MessageHeader header = CreateMessageHeader(MessageType::kStopOfferServiceRequest, 5U, 1U);

    // The payload is invalid for StopOfferServiceRequest
    std::uint8_t bad_payload = 0U;

    EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
        .WillOnce(Invoke(
            [header](auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }))
        .WillOnce(Invoke([&bad_payload](auto, void* buffer, const std::size_t len, auto)
                             -> score::cpp::expected<ssize_t, score::os::Error> {
            std::memcpy(buffer, &bad_payload, len);
            return static_cast<ssize_t>(len);
        }));

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then nullptr is returned (deserialization failed)
    EXPECT_EQ(message, nullptr);
}

TEST_F(MessageFramerFixture, ReceiveMessageSucceedsWithZeroPayload)
{
    // Given a valid header for a message type with zero payload (e.g. StopOfferServiceRequest with no specifier)
    // Using kStopOfferServiceRequest with payload_size = 0 to test the zero-payload path
    MessageHeader header = CreateMessageHeader(MessageType::kStopOfferServiceRequest, 3U, 0U);

    EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
        .WillOnce(Invoke(
            [header](auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }));

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then a valid message is returned (payload_size == 0 skips payload read and deserialization)
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(message->GetType(), MessageType::kStopOfferServiceRequest);
    EXPECT_EQ(message->GetSequenceNumber(), 3U);
}

TEST_F(MessageFramerFixture, ReceiveMessageHandlesPartialHeaderRecv)
{
    // Given a socket that returns the header in two partial reads
    MessageHeader header = CreateMessageHeader(MessageType::kAckResponse, 2U, sizeof(std::uint32_t));

    std::array<std::uint8_t, MessageHeader::kWireSize> header_buf{};
    header.SerializeToBuffer(header_buf.data());

    std::uint32_t payload = 99U;

    {
        ::testing::InSequence seq;
        // First partial header read
        EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
            .WillOnce(Invoke([&header_buf](auto, void* buffer, const std::size_t, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                std::memcpy(buffer, header_buf.data(), kHalfHeaderSize);
                return static_cast<ssize_t>(kHalfHeaderSize);
            }));
        // Second partial header read
        EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
            .WillOnce(Invoke([&header_buf](auto, void* buffer, const std::size_t, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                std::memcpy(buffer, header_buf.data() + kHalfHeaderSize, kRemainingHeaderSize);
                return static_cast<ssize_t>(kRemainingHeaderSize);
            }));
        // Full payload read
        EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
            .WillOnce(Invoke([&payload](auto, void* buffer, const std::size_t len, auto)
                                 -> score::cpp::expected<ssize_t, score::os::Error> {
                std::memcpy(buffer, &payload, len);
                return static_cast<ssize_t>(len);
            }));
    }

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then it succeeds
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(message->GetType(), MessageType::kAckResponse);
    EXPECT_EQ(message->GetSequenceNumber(), 2U);
}

TEST_F(MessageFramerFixture, ReceiveMessageReturnsNullptrWhenPayloadRecvReturnsPeerClosed)
{
    // Given a valid header but payload recv returns 0 (peer closed)
    MessageHeader header = CreateMessageHeader(MessageType::kAckResponse, 1U, sizeof(std::uint32_t));

    EXPECT_CALL(socket_mock_, recv(kSocketFd, _, _, _))
        .WillOnce(Invoke(
            [header](auto, void* buffer, const std::size_t, auto) -> score::cpp::expected<ssize_t, score::os::Error> {
                header.SerializeToBuffer(static_cast<std::uint8_t*>(buffer));
                return static_cast<ssize_t>(MessageHeader::kWireSize);
            }))
        .WillOnce(Return(score::cpp::expected<ssize_t, score::os::Error>{static_cast<ssize_t>(0)}));

    // When ReceiveMessage is called
    auto message = framer_.ReceiveMessage(kSocketFd);

    // Then nullptr is returned
    EXPECT_EQ(message, nullptr);
}

}  // namespace score::mw::com::gateway
