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
#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"
#include "score/mw/com/gateway/transport_layer/sample/messages/message_error.h"

#include "score/mw/com/impl/instance_specifier.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace score::mw::com::gateway
{
namespace
{

constexpr std::size_t kTestBufferSize = 1024U;

TEST(GatewayMessagesTest, ProvideServiceRequestRoundTrip)
{
    auto specifier = impl::InstanceSpecifier::Create(std::string{"SpeedService/Instance42"});
    ASSERT_TRUE(specifier.has_value());

    std::vector<ServiceElementConfiguration> elements{
        {"SpeedEvent", DataTypeSizeInfo{64U, 8U}},
        {"SpeedField", DataTypeSizeInfo{32U, 4U}},
    };

    ProvideServiceRequest original{std::move(specifier).value(), elements, 4U, 8U};

    std::array<std::uint8_t, kTestBufferSize> buffer{};
    const auto size = original.Serialize(buffer);
    ASSERT_GT(size, 0U);

    ProvideServiceRequest deserialized;
    ASSERT_TRUE(deserialized.Deserialize(score::cpp::span<const std::uint8_t>(buffer.data(), size)));

    EXPECT_EQ(deserialized.GetInstanceSpecifier(), "SpeedService/Instance42");
    ASSERT_EQ(deserialized.GetServiceElements().size(), 2U);
    EXPECT_EQ(deserialized.GetServiceElements()[0].element_name, "SpeedEvent");
    EXPECT_EQ(deserialized.GetServiceElements()[0].size_info.Size(), 64U);
    EXPECT_EQ(deserialized.GetServiceElements()[0].size_info.Alignment(), 8U);
    EXPECT_EQ(deserialized.GetServiceElements()[1].element_name, "SpeedField");
    EXPECT_EQ(deserialized.GetServiceElements()[1].size_info.Size(), 32U);
    EXPECT_EQ(deserialized.GetServiceElements()[1].size_info.Alignment(), 4U);
    EXPECT_EQ(deserialized.GetShmControlSize(), 4U);
    EXPECT_EQ(deserialized.GetShmDataSize(), 8U);
}

TEST(GatewayMessagesTest, ProvideServiceRequestEmptyElements)
{
    auto specifier = impl::InstanceSpecifier::Create(std::string{"EmptyService/Inst1"});
    ASSERT_TRUE(specifier.has_value());

    ProvideServiceRequest original{std::move(specifier).value(), {}};

    std::array<std::uint8_t, kTestBufferSize> buffer{};
    const auto size = original.Serialize(buffer);
    ASSERT_GT(size, 0U);

    ProvideServiceRequest deserialized;
    ASSERT_TRUE(deserialized.Deserialize(score::cpp::span<const std::uint8_t>(buffer.data(), size)));

    EXPECT_EQ(deserialized.GetInstanceSpecifier(), "EmptyService/Inst1");
    EXPECT_TRUE(deserialized.GetServiceElements().empty());
}

TEST(GatewayMessagesTest, OfferServiceRequestRoundTrip)
{
    auto specifier = impl::InstanceSpecifier::Create(std::string{"SpeedService/Instance42"});
    ASSERT_TRUE(specifier.has_value());

    OfferServiceRequest original{std::move(specifier).value()};

    std::array<std::uint8_t, kTestBufferSize> buffer{};
    const auto size = original.Serialize(buffer);
    ASSERT_GT(size, 0U);

    OfferServiceRequest deserialized;
    ASSERT_TRUE(deserialized.Deserialize(score::cpp::span<const std::uint8_t>(buffer.data(), size)));

    EXPECT_EQ(deserialized.GetInstanceSpecifier(), "SpeedService/Instance42");
}

TEST(GatewayMessagesTest, StopOfferServiceRequestRoundTrip)
{
    auto specifier = impl::InstanceSpecifier::Create(std::string{"SpeedService/Instance42"});
    ASSERT_TRUE(specifier.has_value());

    StopOfferServiceRequest original{std::move(specifier).value()};

    std::array<std::uint8_t, kTestBufferSize> buffer{};
    const auto size = original.Serialize(buffer);
    ASSERT_GT(size, 0U);

    StopOfferServiceRequest deserialized;
    ASSERT_TRUE(deserialized.Deserialize(score::cpp::span<const std::uint8_t>(buffer.data(), size)));

    EXPECT_EQ(deserialized.GetInstanceSpecifier(), "SpeedService/Instance42");
}

TEST(GatewayMessagesTest, UpdateNotificationRoundTrip)
{
    auto specifier = impl::InstanceSpecifier::Create(std::string{"SpeedService/Instance1"});
    ASSERT_TRUE(specifier.has_value());

    UpdateNotification original{std::move(specifier).value(), impl::ServiceElementType::EVENT, "SpeedEvent"};

    std::array<std::uint8_t, kTestBufferSize> buffer{};
    const auto size = original.Serialize(buffer);
    ASSERT_GT(size, 0U);

    UpdateNotification deserialized;
    ASSERT_TRUE(deserialized.Deserialize(score::cpp::span<const std::uint8_t>(buffer.data(), size)));

    EXPECT_EQ(deserialized.GetInstanceSpecifier(), "SpeedService/Instance1");
    EXPECT_EQ(deserialized.GetElementType(), impl::ServiceElementType::EVENT);
    EXPECT_EQ(deserialized.GetElementName(), "SpeedEvent");
}

TEST(GatewayMessagesTest, RegisterNotificationRequestRoundTrip)
{
    auto specifier = impl::InstanceSpecifier::Create(std::string{"SpeedService/Instance1"});
    ASSERT_TRUE(specifier.has_value());

    RegisterNotificationRequest original{std::move(specifier).value(), impl::ServiceElementType::EVENT, "SpeedEvent"};

    std::array<std::uint8_t, kTestBufferSize> buffer{};
    const auto size = original.Serialize(buffer);
    ASSERT_GT(size, 0U);

    RegisterNotificationRequest deserialized;
    ASSERT_TRUE(deserialized.Deserialize(score::cpp::span<const std::uint8_t>(buffer.data(), size)));

    EXPECT_EQ(deserialized.GetInstanceSpecifier(), "SpeedService/Instance1");
    EXPECT_EQ(deserialized.GetElementType(), impl::ServiceElementType::EVENT);
    EXPECT_EQ(deserialized.GetElementName(), "SpeedEvent");
}

TEST(GatewayMessagesTest, AckResponseRoundTrip)
{
    AckResponse original{42U};

    std::array<std::uint8_t, kTestBufferSize> buffer{};
    const auto size = original.Serialize(buffer);
    ASSERT_GT(size, 0U);

    AckResponse deserialized;
    ASSERT_TRUE(deserialized.Deserialize(score::cpp::span<const std::uint8_t>(buffer.data(), size)));

    EXPECT_EQ(deserialized.GetAckedSequence(), 42U);
}

TEST(GatewayMessagesTest, MessageErrorDomainMessages)
{
    MessageErrorDomain domain;
    EXPECT_EQ(domain.MessageFor(static_cast<score::result::ErrorCode>(MessageErrorc::kBufferTooSmall)),
              "Serialization buffer too small");
    EXPECT_EQ(domain.MessageFor(static_cast<score::result::ErrorCode>(MessageErrorc::kPayloadInvalid)),
              "Format/layout of the message payload is invalid");
    EXPECT_EQ(domain.MessageFor(9999), "unknown message error");
}

TEST(GatewayMessagesTest, MakeErrorConstructsError)
{
    auto err = MakeError(MessageErrorc::kBufferTooSmall, "test");
    EXPECT_EQ(*err, static_cast<score::result::ErrorCode>(MessageErrorc::kBufferTooSmall));
    EXPECT_EQ(err.UserMessage(), "test");
}

TEST(GatewayMessagesTest, SequenceNumberIsSetAndRetrievedCorrectly)
{
    const auto sequence_number = 123U;
    // Given a ProvideServiceRequest with a specific instance specifier and empty service elements
    ProvideServiceRequest request{impl::InstanceSpecifier::Create("TestService/Instance1").value(), {}};
    EXPECT_EQ(request.GetSequenceNumber(), 0U);
    // when setting the sequence number
    request.SetSequenceNumber(sequence_number);
    // then the sequence number can be retrieved correctly
    EXPECT_EQ(request.GetSequenceNumber(), sequence_number);
}

TEST(GatewayMessagesTest, MessageTypeCanBeRetrievedCorrectly)
{
    // Given a ProvideServiceRequest
    ProvideServiceRequest request{impl::InstanceSpecifier::Create("TestService/Instance1").value(), {}};
    // when retrieving the message type
    auto type = request.GetType();
    // then the message type is correct
    EXPECT_EQ(type, MessageType::kProvideServiceRequest);
}

TEST(GatewayMessagesTest, MessageHeaderCanBeRetrievedCorrectly)
{
    // Given a ProvideServiceRequest with a specific instance specifier and empty service elements
    ProvideServiceRequest request{impl::InstanceSpecifier::Create("TestService/Instance1").value(), {}};
    // when retrieving the message header
    const auto& header = request.GetHeader();
    // then the message header contains the correct type and default sequence and payload size
    EXPECT_EQ(header.type, MessageType::kProvideServiceRequest);
    EXPECT_EQ(header.sequence, 0U);
    EXPECT_EQ(header.payload_size, 0U);
}

TEST(GatewayMessagesTest, SerializeWithTooSmallBufferReturnsSizeZero)
{
    // Given a ProvideServiceRequest with a specific instance specifier and empty service elements
    auto specifier = impl::InstanceSpecifier::Create(std::string{"SpeedService/Instance42"});
    ASSERT_TRUE(specifier.has_value());

    ProvideServiceRequest service_request{std::move(specifier).value(), {}};
    // when serializing with an intentionally too small buffer
    std::array<std::uint8_t, 1U> buffer{};
    const auto size = service_request.Serialize(buffer);
    // then the serialization fails and returns size 0
    EXPECT_EQ(size, 0U);
}

TEST(GatewayMessagesTest, SerializeWithVectorExceedingUint16MaxReturnsZero)
{
    // Given a ProvideServiceRequest with more than 65535 ServiceElementConfiguration elements
    auto specifier = impl::InstanceSpecifier::Create(std::string{"SpeedService/Instance42"});
    ASSERT_TRUE(specifier.has_value());

    constexpr std::size_t kTooManyElements = static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1U;
    std::vector<ServiceElementConfiguration> elements(kTooManyElements,
                                                      ServiceElementConfiguration{"A", DataTypeSizeInfo{8U, 8U}});

    ProvideServiceRequest request{std::move(specifier).value(), std::move(elements)};

    // when serializing with a large buffer so that the initial size check will pass
    constexpr std::size_t kLargeBufferSize = 4U * 1024U * 1024U;
    std::vector<std::uint8_t> buffer(kLargeBufferSize, 0U);
    const auto size = request.Serialize(score::cpp::span<std::uint8_t>(buffer.data(), buffer.size()));

    // then the serialization fails (Serialize of vector checks for max. size of elements) and returns size 0
    EXPECT_EQ(size, 0U);
}

TEST(GatewayMessageTest, ConstructUnregisterNotificationRequestWithoutConstructorArguments)
{
    // When constructing an UnregisterNotificationRequest without constructor arguments
    UnregisterNotificationRequest request;

    // then the message header type should be set to kUnregisterNotificationRequest
    EXPECT_EQ(request.GetHeader().type, MessageType::kUnregisterNotificationRequest);
}

TEST(GatewayMessageTest, AckedSequenceNumberCanBeSetAtAckResponse)
{
    // Given an AckResponse message
    AckResponse ack_response;

    // when setting the sequence number
    const auto sequence_number = 99U;
    ack_response.SetAckedSequence(sequence_number);

    // then this sequence number will be returned when calling the matching getter
    EXPECT_EQ(ack_response.GetAckedSequence(), sequence_number);
}

TEST(GatewayMessageTest, MessageHeaderCanBeSerializedAndDeserialized)
{
    // Given a MessageHeader with specific values
    MessageHeader header;
    header.type = MessageType::kOfferServiceRequest;
    header.sequence = 42U;
    header.payload_size = 128U;

    // when serializing to a buffer and deserializing it
    std::array<std::uint8_t, MessageHeader::kWireSize> buffer{};
    header.SerializeToBuffer(buffer.data());

    MessageHeader deserialized_header;
    deserialized_header.DeserializeFromBuffer(buffer.data());

    // then the deserialized header should have the same values as the original
    EXPECT_EQ(deserialized_header.type, header.type);
    EXPECT_EQ(deserialized_header.sequence, header.sequence);
    EXPECT_EQ(deserialized_header.payload_size, header.payload_size);
}

TEST(GatewayMessageTest, RequestTypeCanBeConvertedToMessageType)
{
    // Given a MessageType that is a request type
    RequestType request_type = RequestType::kProvideService;

    // when converting it to a message type
    const auto message_type = ToMessageType(request_type);

    // then it should be of the expected message type
    EXPECT_EQ(message_type, MessageType::kProvideServiceRequest);
}

TEST(GatewayMessageTest, ResponseTypeCanBeConvertedToMessageType)
{
    // Given a MessageType that is a response type
    ResponseType response_type = ResponseType::kAck;

    // when converting it to a message type
    const auto message_type = ToMessageType(response_type);

    // then it should be of the expected message type
    EXPECT_EQ(message_type, MessageType::kAckResponse);
}

TEST(GatewayMessageTest, NotificationTypeCanBeConvertedToMessageType)
{
    // Given a MessageType that is a notification type
    NotificationType notification_type = NotificationType::kUpdate;

    // when converting it to a message type
    const auto message_type = ToMessageType(notification_type);

    // then it should be of the expected message type
    EXPECT_EQ(message_type, MessageType::kUpdateNotification);
}

TEST(GatewayMessageTest, IsRequestIdentifiesRequestTypes)
{
    EXPECT_TRUE(IsRequest(MessageType::kProvideServiceRequest));
    EXPECT_TRUE(IsRequest(MessageType::kOfferServiceRequest));
    EXPECT_TRUE(IsRequest(MessageType::kStopOfferServiceRequest));
    EXPECT_TRUE(IsRequest(MessageType::kRegisterNotificationRequest));
    EXPECT_TRUE(IsRequest(MessageType::kUnregisterNotificationRequest));
    EXPECT_FALSE(IsRequest(MessageType::kUpdateNotification));
    EXPECT_FALSE(IsRequest(MessageType::kAckResponse));
}

TEST(GatewayMessageTest, IsNotificationIdentifiesNotificationTypes)
{
    EXPECT_FALSE(IsNotification(MessageType::kProvideServiceRequest));
    EXPECT_FALSE(IsNotification(MessageType::kOfferServiceRequest));
    EXPECT_FALSE(IsNotification(MessageType::kStopOfferServiceRequest));
    EXPECT_FALSE(IsNotification(MessageType::kRegisterNotificationRequest));
    EXPECT_FALSE(IsNotification(MessageType::kUnregisterNotificationRequest));
    EXPECT_TRUE(IsNotification(MessageType::kUpdateNotification));
    EXPECT_FALSE(IsNotification(MessageType::kAckResponse));
}

TEST(GatewayMessageTest, IsResponseIdentifiesResponseTypes)
{
    EXPECT_FALSE(IsResponse(MessageType::kProvideServiceRequest));
    EXPECT_FALSE(IsResponse(MessageType::kOfferServiceRequest));
    EXPECT_FALSE(IsResponse(MessageType::kStopOfferServiceRequest));
    EXPECT_FALSE(IsResponse(MessageType::kRegisterNotificationRequest));
    EXPECT_FALSE(IsResponse(MessageType::kUnregisterNotificationRequest));
    EXPECT_FALSE(IsResponse(MessageType::kUpdateNotification));
    EXPECT_TRUE(IsResponse(MessageType::kAckResponse));
}

TEST(GatewayMessageTest, RequiresResponseIdentifiesRequestTypes)
{
    EXPECT_TRUE(RequiresResponse(MessageType::kProvideServiceRequest));
    EXPECT_TRUE(RequiresResponse(MessageType::kOfferServiceRequest));
    EXPECT_TRUE(RequiresResponse(MessageType::kStopOfferServiceRequest));
    EXPECT_TRUE(RequiresResponse(MessageType::kRegisterNotificationRequest));
    EXPECT_TRUE(RequiresResponse(MessageType::kUnregisterNotificationRequest));
    EXPECT_FALSE(RequiresResponse(MessageType::kUpdateNotification));
    EXPECT_FALSE(RequiresResponse(MessageType::kAckResponse));
}
}  // namespace
}  // namespace score::mw::com::gateway
