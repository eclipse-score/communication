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
#include "score/someip/routing_module/event_handler/someip_message_header_parser.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace score::someip::routing_module
{
namespace
{

using Header = std::array<std::uint8_t, kSomeipHeaderSize>;

Header MakeHeader(const std::uint32_t length,
                  const std::uint8_t protocol_version = kSomeipProtocolVersion,
                  const std::uint8_t message_type = kSomeipNotificationMessageType)
{
    Header header{0x12U,
                  0x34U,
                  0x80U,
                  0x01U,
                  0U,
                  0U,
                  0U,
                  0U,
                  0xABU,
                  0xCDU,
                  0x12U,
                  0x34U,
                  protocol_version,
                  0x03U,
                  message_type,
                  0x00U};
    header[4U] = static_cast<std::uint8_t>(length >> 24U);
    header[5U] = static_cast<std::uint8_t>(length >> 16U);
    header[6U] = static_cast<std::uint8_t>(length >> 8U);
    header[7U] = static_cast<std::uint8_t>(length);
    return header;
}

void Trace(const char* const requirements, const char* const given, const char* const when, const char* const then)
{
    ::testing::Test::RecordProperty("lobster-tracing", requirements);
    ::testing::Test::RecordProperty("given", given);
    ::testing::Test::RecordProperty("when", when);
    ::testing::Test::RecordProperty("then", then);
}

TEST(SomeipMessageHeaderParserTest, ParsesHeaderFieldsAndPayloadSize)
{
    Trace("RoutingModule.CR_RoutingModule_ReceiveMetadata",
          "a complete SOME/IP notification header with a four-byte payload",
          "the header is parsed",
          "service, event, client, session, versions, type, return code and payload size are decoded");
    const Header header = MakeHeader(12U);

    const auto parsed = SomeipMessageHeaderParser::Parse(header, 20U);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->service_id, 0x1234U);
    EXPECT_EQ(parsed->method_or_event_id, 0x8001U);
    EXPECT_EQ(parsed->client_id, 0xABCDU);
    EXPECT_EQ(parsed->session_id, 0x1234U);
    EXPECT_EQ(parsed->protocol_version, kSomeipProtocolVersion);
    EXPECT_EQ(parsed->interface_version, 0x03U);
    EXPECT_EQ(parsed->message_type, kSomeipNotificationMessageType);
    EXPECT_EQ(parsed->return_code, 0U);
    EXPECT_EQ(parsed->message_size, 20U);
    EXPECT_EQ(parsed->payload_size, 4U);
}

TEST(SomeipMessageHeaderParserTest, AcceptsAnEmptyPayload)
{
    Trace("RoutingModule.CR_RoutingModule_EmptyPayload",
          "a complete SOME/IP notification header with the minimum length",
          "the header is parsed",
          "the payload size is zero and the message size is the header size");
    const Header header = MakeHeader(kSomeipMinimumLength);

    const auto parsed = SomeipMessageHeaderParser::Parse(header, kSomeipHeaderSize);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->payload_size, 0U);
    EXPECT_EQ(parsed->message_size, kSomeipHeaderSize);
}

TEST(SomeipMessageHeaderParserTest, RejectsAnIncompleteHeader)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidFraming",
          "fewer than 16 bytes remain in a UDP datagram",
          "a SOME/IP header is parsed",
          "parsing fails with an invalid-header-size error");
    const Header header = MakeHeader(kSomeipMinimumLength);

    const auto short_input = SomeipMessageHeaderParser::Parse(
        score::cpp::span<const std::uint8_t>{header.data(), kSomeipHeaderSize - 1U}, kSomeipHeaderSize - 1U);
    const auto short_datagram = SomeipMessageHeaderParser::Parse(header, kSomeipHeaderSize - 1U);

    ASSERT_FALSE(short_input.has_value());
    EXPECT_EQ(short_input.error(), SomeipHeaderParseError::kInvalidHeaderSize);
    ASSERT_FALSE(short_datagram.has_value());
    EXPECT_EQ(short_datagram.error(), SomeipHeaderParseError::kInvalidHeaderSize);
}

TEST(SomeipMessageHeaderParserTest, RejectsAnUnsupportedProtocolVersion)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidFraming",
          "a complete SOME/IP message whose protocol version is not one",
          "the header is parsed",
          "parsing fails with an unsupported-protocol-version error");
    const Header header = MakeHeader(kSomeipMinimumLength, 2U);

    const auto parsed = SomeipMessageHeaderParser::Parse(header, kSomeipHeaderSize);

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error(), SomeipHeaderParseError::kUnsupportedProtocolVersion);
}

TEST(SomeipMessageHeaderParserTest, RejectsALengthBelowTheSOMEIPMinimum)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidFraming",
          "a complete SOME/IP message with a length below eight",
          "the header is parsed",
          "parsing fails with a length-too-short error");
    const Header header = MakeHeader(kSomeipMinimumLength - 1U);

    const auto parsed = SomeipMessageHeaderParser::Parse(header, kSomeipHeaderSize);

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error(), SomeipHeaderParseError::kLengthTooShort);
}

TEST(SomeipMessageHeaderParserTest, RejectsAMessageThatExceedsTheDatagram)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidFraming",
          "a SOME/IP message whose declared length exceeds the bytes remaining in the UDP datagram",
          "the header is parsed",
          "parsing fails with a message-exceeds-datagram error");
    const Header header = MakeHeader(kSomeipMinimumLength + 1U);

    const auto parsed = SomeipMessageHeaderParser::Parse(header, kSomeipHeaderSize);

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error(), SomeipHeaderParseError::kMessageExceedsDatagram);
}

TEST(SomeipMessageHeaderParserTest, LeavesMessageTypeFilteringToTheCaller)
{
    Trace("RoutingModule.CR_RoutingModule_NotificationOnly",
          "a complete SOME/IP response header",
          "the header is parsed",
          "the parsed message type is available to the caller for filtering");
    constexpr std::uint8_t kResponseMessageType{0x80U};
    const Header header = MakeHeader(kSomeipMinimumLength, kSomeipProtocolVersion, kResponseMessageType);

    const auto parsed = SomeipMessageHeaderParser::Parse(header, kSomeipHeaderSize);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->message_type, kResponseMessageType);
}

}  // namespace
}  // namespace score::someip::routing_module
