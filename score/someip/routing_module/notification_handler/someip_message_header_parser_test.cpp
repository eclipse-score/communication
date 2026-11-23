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
#include "score/someip/routing_module/notification_handler/someip_message_header_parser.h"
#include "score/someip/routing_module/notification_handler/someip_datagram_parser.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

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

void AppendMessage(std::vector<std::uint8_t>& datagram, const Header& header, const std::vector<std::uint8_t>& payload)
{
    datagram.insert(datagram.end(), header.begin(), header.end());
    datagram.insert(datagram.end(), payload.begin(), payload.end());
}

class SegmentedPacketReader final
{
  public:
        explicit SegmentedPacketReader(const std::vector<std::uint8_t>& bytes,
                                                                     const std::size_t segment_size = 5U,
                                                                     const bool fail_reads = false) noexcept
                : bytes_{bytes}, segment_size_{segment_size}, fail_reads_{fail_reads}
    {
    }

    std::size_t Size() const noexcept
    {
        return bytes_.size();
    }

    bool Copy(const std::size_t offset, const score::cpp::span<std::uint8_t> destination) const noexcept
    {
        if (fail_reads_ || (offset > bytes_.size()) || (destination.size() > (bytes_.size() - offset)))
        {
            return false;
        }
        std::size_t copied{0U};
        while (copied < destination.size())
        {
            const std::size_t absolute_offset = offset + copied;
            const std::size_t bytes_until_segment_end = segment_size_ - (absolute_offset % segment_size_);
            const std::size_t copy_size = std::min(destination.size() - copied, bytes_until_segment_end);
            std::copy_n(bytes_.cbegin() + static_cast<std::ptrdiff_t>(absolute_offset), copy_size,
                        destination.begin() + static_cast<std::ptrdiff_t>(copied));
            copied += copy_size;
        }
        return true;
    }

  private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t segment_size_;
    bool fail_reads_;
};

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

TEST(SomeipDatagramParserTest, ParsesConcatenatedNotifications)
{
    Trace("RoutingModule.CR_RoutingModule_ConcatenatedMessages",
          "a UDP datagram with two concatenated notifications and different payload sizes",
          "the datagram is iterated",
            "both notification headers and payload offsets are returned in order, then iteration ends");
    std::vector<std::uint8_t> datagram{};
    AppendMessage(datagram, MakeHeader(10U), {0xAAU, 0xBBU});
    AppendMessage(datagram, MakeHeader(kSomeipMinimumLength), {});
        SegmentedPacketReader packet{datagram};
    SomeipDatagramParser parser{};

    const auto first = parser.NextNotification(packet);
    const auto second = parser.NextNotification(packet);
    const auto end = parser.NextNotification(packet);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(first->has_value());
    EXPECT_EQ(first->value().header.message_size, kSomeipHeaderSize + 2U);
    EXPECT_EQ(first->value().header.payload_size, 2U);
    EXPECT_EQ(first->value().payload_offset, kSomeipHeaderSize);
    std::array<std::uint8_t, 2U> payload{};
    ASSERT_TRUE(packet.Copy(first->value().payload_offset, payload));
    EXPECT_EQ(payload[0U], 0xAAU);
    EXPECT_EQ(payload[1U], 0xBBU);
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(second->has_value());
    EXPECT_EQ(second->value().header.payload_size, 0U);
    EXPECT_EQ(second->value().payload_offset, (kSomeipHeaderSize * 2U) + 2U);
    ASSERT_TRUE(end.has_value());
    EXPECT_FALSE(end->has_value());
}

TEST(SomeipDatagramParserTest, SkipsNonNotificationMessages)
{
    Trace("RoutingModule.CR_RoutingModule_NotificationOnly",
          "a UDP datagram with a response followed by a notification",
          "the datagram is iterated for notifications",
          "the response is skipped and only the notification is returned");
    std::vector<std::uint8_t> datagram{};
    AppendMessage(datagram, MakeHeader(kSomeipMinimumLength, kSomeipProtocolVersion, 0x80U), {});
    AppendMessage(datagram, MakeHeader(kSomeipMinimumLength), {});
        SegmentedPacketReader packet{datagram};
    SomeipDatagramParser parser{};

    const auto notification = parser.NextNotification(packet);

    ASSERT_TRUE(notification.has_value());
    ASSERT_TRUE(notification->has_value());
    EXPECT_EQ(notification->value().header.message_type, kSomeipNotificationMessageType);
    EXPECT_FALSE(parser.NextNotification(packet).value().has_value());
}

TEST(SomeipDatagramParserTest, RejectsTruncatedDataAfterCompleteMessages)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidFraming",
          "a complete notification followed by fewer than 16 trailing bytes",
          "the datagram is iterated",
          "the notification is returned first, then iteration reports an invalid-header-size error");
    std::vector<std::uint8_t> datagram{};
    AppendMessage(datagram, MakeHeader(kSomeipMinimumLength), {});
    datagram.insert(datagram.end(), {0x12U, 0x34U, 0x00U, 0x01U, 0x00U});
        SegmentedPacketReader packet{datagram};
    SomeipDatagramParser parser{};

    const auto notification = parser.NextNotification(packet);
    const auto malformed = parser.NextNotification(packet);

    ASSERT_TRUE(notification.has_value());
    ASSERT_TRUE(notification->has_value());
    ASSERT_FALSE(malformed.has_value());
    EXPECT_EQ(malformed.error(), SomeipHeaderParseError::kInvalidHeaderSize);
}

TEST(SomeipDatagramParserTest, ReportsPacketReadFailure)
{
    Trace("RoutingModule.CR_RoutingModule_InvalidFraming",
          "a packet reader that cannot copy a SOME/IP header",
          "the datagram is iterated",
          "iteration fails with a packet-read error");
    const std::vector<std::uint8_t> datagram(kSomeipHeaderSize, 0U);
        SegmentedPacketReader packet{datagram, 5U, true};
    SomeipDatagramParser parser{};

    const auto notification = parser.NextNotification(packet);

    ASSERT_FALSE(notification.has_value());
    EXPECT_EQ(notification.error(), SomeipHeaderParseError::kPacketReadFailed);
}

}  // namespace
}  // namespace score::someip::routing_module
