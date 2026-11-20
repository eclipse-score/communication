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
#include "score/someip/routing_control_protocol/routing_control_protocol.h"

#include "score/someip/routing_control_protocol/wire_format.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::someip::routing_control_protocol
{
namespace
{

using detail::kHeaderSize;
using Buffer = std::array<std::uint8_t, kMaxMessageSize>;

static_assert(noexcept(Encode(std::declval<const Message&>(), std::declval<score::cpp::span<std::uint8_t>>())));
static_assert(noexcept(Decode(std::declval<score::cpp::span<const std::uint8_t>>())));
static_assert(std::is_nothrow_copy_constructible_v<Message>);
static_assert(std::is_nothrow_move_constructible_v<Message>);

void Trace(const char* const requirements, const char* const given, const char* const when, const char* const then)
{
    ::testing::Test::RecordProperty("lobster-tracing", requirements);
    ::testing::Test::RecordProperty("given", given);
    ::testing::Test::RecordProperty("when", when);
    ::testing::Test::RecordProperty("then", then);
}

Message AddRemoteService()
{
    Message message{};
    message.opcode = Opcode::kAddRemoteService;
    message.service = 0x1234U;
    message.instance = 0x0001U;
    message.local_port = 30501U;
    message.remote_port = 30502U;
    message.remote_ip = Ipv4Address{10U, 0U, 3U, 2U};
    message.events = EventIds{0x8001U, 0x8002U};
    return message;
}

Message AddLocalService()
{
    Message message{};
    message.opcode = Opcode::kAddLocalService;
    message.service = 0xB001U;
    message.instance = 0x0002U;
    message.local_port = 30720U;
    message.major = 3U;
    message.events = EventIds{0x8001U, 0x8002U, 0x8003U};
    message.fields = EventIds{0x8003U};
    return message;
}

Message WithEvent(const Opcode opcode)
{
    Message message{};
    message.opcode = opcode;
    message.service = 0xB002U;
    message.instance = 0x0003U;
    message.event = 0x8004U;
    return message;
}

Message WithRemoteSubscriber(const Opcode opcode)
{
    Message message = WithEvent(opcode);
    message.remote_port = 30721U;
    message.remote_ip = Ipv4Address{192U, 168U, 0U, 7U};
    return message;
}

Message WithServiceInstance(const Opcode opcode)
{
    Message message{};
    message.opcode = opcode;
    message.service = 0x1234U;
    message.instance = 0x0001U;
    return message;
}

std::vector<Message> AllCommands()
{
    return {AddRemoteService(),
            WithServiceInstance(Opcode::kRemoveRemoteService),
            WithEvent(Opcode::kAddLocalSubscription),
            WithEvent(Opcode::kRemoveLocalSubscription),
            AddLocalService(),
            WithServiceInstance(Opcode::kRemoveLocalService),
            WithRemoteSubscriber(Opcode::kAddRemoteSubscription),
            WithRemoteSubscriber(Opcode::kRemoveRemoteSubscription),
            Message{Opcode::kShutdown}};
}

void ExpectEqual(const Message& actual, const Message& expected)
{
    EXPECT_EQ(actual.opcode, expected.opcode);
    EXPECT_EQ(actual.service, expected.service);
    EXPECT_EQ(actual.instance, expected.instance);
    EXPECT_EQ(actual.event, expected.event);
    EXPECT_EQ(actual.local_port, expected.local_port);
    EXPECT_EQ(actual.remote_port, expected.remote_port);
    EXPECT_EQ(actual.remote_ip, expected.remote_ip);
    EXPECT_EQ(actual.major, expected.major);
    EXPECT_EQ(std::vector<std::uint16_t>(actual.events.cbegin(), actual.events.cend()),
              std::vector<std::uint16_t>(expected.events.cbegin(), expected.events.cend()));
    EXPECT_EQ(std::vector<std::uint16_t>(actual.fields.cbegin(), actual.fields.cend()),
              std::vector<std::uint16_t>(expected.fields.cbegin(), expected.fields.cend()));
}

std::vector<std::uint8_t> EncodeToVector(const Message& message)
{
    Buffer buffer{};
    const auto size = Encode(message, buffer);
    EXPECT_TRUE(size.has_value());
    return std::vector<std::uint8_t>(buffer.cbegin(), buffer.cbegin() + static_cast<std::ptrdiff_t>(size.value()));
}

score::cpp::expected<Message, ErrorCode> DecodeVector(const std::vector<std::uint8_t>& data)
{
    return Decode(score::cpp::span<const std::uint8_t>{data.data(), data.size()});
}

TEST(RoutingControlProtocolTest, EveryRoutingCommandRoundTrips)
{
    Trace(
        "RoutingControlProtocol.CR_RoutingControlProtocol_RoutingCommands,"
        "RoutingControlProtocol.CR_RoutingControlProtocol_Lossless",
        "a valid routing command for each opcode",
        "the command is encoded and the result is decoded",
        "the decoded command equals the encoded one");

    for (const Message& message : AllCommands())
    {
        const auto decoded = DecodeVector(EncodeToVector(message));

        ASSERT_TRUE(decoded.has_value());
        ExpectEqual(decoded.value(), message);
    }
}

TEST(RoutingControlProtocolTest, AddRemoteServiceIsEncodedInWireLayout)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_WireFormat",
          "an add-remote-service command with two events",
          "the command is encoded",
          "the bytes match the little-endian wire layout starting with version and opcode");

    const std::vector<std::uint8_t> expected{0x01U, 0x01U, 0x34U, 0x12U, 0x01U, 0x00U, 0x00U, 0x00U, 0x25U,
                                             0x77U, 0x26U, 0x77U, 10U,   0U,    3U,    2U,    0x00U, 0x00U,
                                             0x02U, 0x00U, 0x00U, 0x00U, 0x01U, 0x80U, 0x02U, 0x80U};

    EXPECT_EQ(EncodeToVector(AddRemoteService()), expected);
}

TEST(RoutingControlProtocolTest, LocalServiceIsEncodedInWireLayout)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_WireFormat",
          "an add-local-service command with three events, one of them a field",
          "the command is encoded",
          "major version, counts and identifiers are at their wire layout positions");

    const auto bytes = EncodeToVector(AddLocalService());

    ASSERT_EQ(bytes.size(), kHeaderSize + 8U);
    EXPECT_EQ(bytes[16U], 3U);
    EXPECT_EQ(bytes[18U], 3U);
    EXPECT_EQ(bytes[20U], 1U);
    EXPECT_EQ(bytes[kHeaderSize + 6U], 0x03U);
    EXPECT_EQ(bytes[kHeaderSize + 7U], 0x80U);
}

TEST(RoutingControlProtocolTest, DecodeRejectsOtherVersion)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_Versioning",
          "an encoded routing command whose version byte differs from the supported version",
          "the command is decoded",
          "decoding fails with an unsupported-version error");

    for (const std::uint8_t version : std::array<std::uint8_t, 2U>{static_cast<std::uint8_t>(kProtocolVersion - 1U),
                                                                   static_cast<std::uint8_t>(kProtocolVersion + 1U)})
    {
        auto bytes = EncodeToVector(WithServiceInstance(Opcode::kRemoveRemoteService));
        bytes[0U] = version;

        const auto decoded = DecodeVector(bytes);

        ASSERT_FALSE(decoded.has_value());
        EXPECT_EQ(decoded.error(), ErrorCode::kUnsupportedVersion);
    }
}

TEST(RoutingControlProtocolTest, MaximumCommandFitsMaximumMessageSize)
{
    Trace(
        "RoutingControlProtocol.CR_RoutingControlProtocol_BoundedSize,"
        "RoutingControlProtocol.CR_RoutingControlProtocol_Lossless",
        "an add-local-service command with 64 events that are all fields",
        "the command is encoded and decoded",
        "the encoded size equals the maximum message size and the command round-trips");

    Message message = AddLocalService();
    message.events.clear();
    message.fields.clear();
    for (std::uint16_t index = 0U; index < kMaxEvents; ++index)
    {
        message.events.push_back(static_cast<std::uint16_t>(0x8000U + index));
        message.fields.push_back(static_cast<std::uint16_t>(0x8000U + index));
    }

    const auto bytes = EncodeToVector(message);
    const auto decoded = DecodeVector(bytes);

    EXPECT_EQ(bytes.size(), kMaxMessageSize);
    ASSERT_TRUE(decoded.has_value());
    ExpectEqual(decoded.value(), message);
}

TEST(RoutingControlProtocolTest, DecodeRejectsTooManyIdentifiers)
{
    Trace(
        "RoutingControlProtocol.CR_RoutingControlProtocol_BoundedSize,"
        "RoutingControlProtocol.CR_RoutingControlProtocol_RejectMalformed",
        "an encoded routing command announcing 65 event or 65 field identifiers",
        "the command is decoded",
        "decoding fails with a too-many-events error");

    for (const std::size_t count_offset : std::array<std::size_t, 2U>{18U, 20U})
    {
        auto bytes = EncodeToVector(AddLocalService());
        bytes[count_offset] = static_cast<std::uint8_t>(kMaxEvents + 1U);
        bytes[count_offset + 1U] = 0U;

        const auto decoded = DecodeVector(bytes);

        ASSERT_FALSE(decoded.has_value());
        EXPECT_EQ(decoded.error(), ErrorCode::kTooManyEvents);
    }
}

TEST(RoutingControlProtocolTest, DecodeRejectsWrongSize)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_RejectMalformed",
          "an encoded add-remote-service command that is empty, shorter than the header, one byte short or one byte "
          "too long",
          "the command is decoded",
          "decoding fails with an invalid-size error");

    const auto bytes = EncodeToVector(AddRemoteService());
    std::vector<std::vector<std::uint8_t>> malformed{
        {},
        std::vector<std::uint8_t>(bytes.cbegin(), bytes.cbegin() + static_cast<std::ptrdiff_t>(kHeaderSize - 1U)),
        std::vector<std::uint8_t>(bytes.cbegin(), bytes.cend() - 1)};
    malformed.push_back(bytes);
    malformed.back().push_back(0U);

    for (const auto& data : malformed)
    {
        const auto decoded = DecodeVector(data);

        ASSERT_FALSE(decoded.has_value());
        EXPECT_EQ(decoded.error(), ErrorCode::kInvalidSize);
    }
}

TEST(RoutingControlProtocolTest, DecodeRejectsUnknownOpcode)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_RejectMalformed",
          "an encoded routing command with opcode 0x00, 0x0A or 0x81",
          "the command is decoded",
          "decoding fails with an unknown-opcode error");

    for (const std::uint8_t opcode : std::array<std::uint8_t, 3U>{0x00U, 0x0AU, 0x81U})
    {
        auto bytes = EncodeToVector(WithServiceInstance(Opcode::kRemoveRemoteService));
        bytes[1U] = opcode;

        const auto decoded = DecodeVector(bytes);

        ASSERT_FALSE(decoded.has_value());
        EXPECT_EQ(decoded.error(), ErrorCode::kUnknownOpcode);
    }
}

TEST(RoutingControlProtocolTest, DecodeRejectsNonZeroReservedByte)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_RejectMalformed",
          "an encoded routing command with a non-zero reserved byte",
          "the command is decoded",
          "decoding fails with a reserved-not-zero error");

    auto bytes = EncodeToVector(WithServiceInstance(Opcode::kRemoveRemoteService));
    bytes[17U] = 1U;

    const auto decoded = DecodeVector(bytes);

    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error(), ErrorCode::kReservedNotZero);
}

TEST(RoutingControlProtocolTest, EncodeRejectsParameterNotUsedByOpcode)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_CommandConsistency",
          "routing commands that each set one parameter their opcode does not use",
          "the commands are encoded",
          "encoding fails with an unexpected-parameter error and the buffer is unchanged");

    std::vector<Message> invalid{};
    invalid.push_back(WithServiceInstance(Opcode::kRemoveRemoteService));
    invalid.back().event = 1U;
    invalid.push_back(WithServiceInstance(Opcode::kRemoveLocalService));
    invalid.back().major = 1U;
    invalid.push_back(WithEvent(Opcode::kAddLocalSubscription));
    invalid.back().local_port = 1U;
    invalid.push_back(AddLocalService());
    invalid.back().remote_ip = Ipv4Address{1U, 0U, 0U, 0U};
    invalid.push_back(AddRemoteService());
    invalid.back().major = 1U;
    invalid.push_back(WithRemoteSubscriber(Opcode::kAddRemoteSubscription));
    invalid.back().events.push_back(1U);
    invalid.push_back(AddRemoteService());
    invalid.back().fields.push_back(0x8001U);
    invalid.push_back(WithEvent(Opcode::kRemoveLocalSubscription));
    invalid.back().remote_port = 1U;
    invalid.push_back(Message{Opcode::kShutdown});
    invalid.back().service = 1U;
    invalid.push_back(Message{Opcode::kShutdown});
    invalid.back().instance = 1U;

    for (const Message& message : invalid)
    {
        Buffer buffer{};

        const auto size = Encode(message, buffer);

        ASSERT_FALSE(size.has_value());
        EXPECT_EQ(size.error(), ErrorCode::kUnexpectedParameter);
        EXPECT_EQ(buffer, Buffer{});
    }
}

TEST(RoutingControlProtocolTest, DecodeRejectsParameterNotUsedByOpcode)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_CommandConsistency",
          "an encoded remove-remote-service command with a non-zero local port",
          "the command is decoded",
          "decoding fails with an unexpected-parameter error");

    auto bytes = EncodeToVector(WithServiceInstance(Opcode::kRemoveRemoteService));
    bytes[8U] = 1U;

    const auto decoded = DecodeVector(bytes);

    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error(), ErrorCode::kUnexpectedParameter);
}

TEST(RoutingControlProtocolTest, FieldMustBeOneOfTheEvents)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_CommandConsistency",
          "an add-local-service command whose field identifier is not one of its events",
          "the command is encoded, and its valid encoding is altered and decoded",
          "both fail with a field-not-in-events error");

    Message message = AddLocalService();
    message.fields = EventIds{0x9000U};
    Buffer buffer{};
    const auto size = Encode(message, buffer);

    auto bytes = EncodeToVector(AddLocalService());
    bytes[bytes.size() - 2U] = 0x00U;
    bytes[bytes.size() - 1U] = 0x90U;
    const auto decoded = DecodeVector(bytes);

    ASSERT_FALSE(size.has_value());
    EXPECT_EQ(size.error(), ErrorCode::kFieldNotInEvents);
    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error(), ErrorCode::kFieldNotInEvents);
}

TEST(RoutingControlProtocolTest, EncodeRejectsUnknownOpcode)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_RoutingCommands",
          "a routing command with an opcode value outside the defined opcodes",
          "the command is encoded",
          "encoding fails with an unknown-opcode error");

    Message message = WithServiceInstance(Opcode::kRemoveRemoteService);
    message.opcode = static_cast<Opcode>(0x0AU);
    Buffer buffer{};

    const auto size = Encode(message, buffer);

    ASSERT_FALSE(size.has_value());
    EXPECT_EQ(size.error(), ErrorCode::kUnknownOpcode);
}

TEST(RoutingControlProtocolTest, EncodeRejectsTooSmallBuffer)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_BufferBounds",
          "an add-remote-service command and a buffer one byte smaller than its encoding",
          "the command is encoded into the buffer",
          "encoding fails with a buffer-too-small error and the buffer is unchanged");

    const std::size_t required = EncodeToVector(AddRemoteService()).size();
    std::vector<std::uint8_t> buffer(required - 1U, 0xAAU);

    const auto size = Encode(AddRemoteService(), score::cpp::span<std::uint8_t>{buffer.data(), buffer.size()});

    ASSERT_FALSE(size.has_value());
    EXPECT_EQ(size.error(), ErrorCode::kBufferTooSmall);
    EXPECT_EQ(buffer, std::vector<std::uint8_t>(required - 1U, 0xAAU));
}

TEST(RoutingControlProtocolTest, EncodeFitsExactBuffer)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_BufferBounds",
          "an add-remote-service command and a buffer exactly as large as its encoding",
          "the command is encoded into the buffer",
          "encoding succeeds and reports the buffer size");

    const std::size_t required = EncodeToVector(AddRemoteService()).size();
    std::vector<std::uint8_t> buffer(required, 0U);

    const auto size = Encode(AddRemoteService(), score::cpp::span<std::uint8_t>{buffer.data(), buffer.size()});

    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(size.value(), required);
}

void ExpectRejectedOrCanonical(const std::vector<std::uint8_t>& data)
{
    const auto decoded = DecodeVector(data);
    if (decoded.has_value())
    {
        EXPECT_EQ(EncodeToVector(decoded.value()), data);
    }
}

TEST(RoutingControlProtocolTest, DecodeHandlesEverySingleByteCorruption)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_RobustDecoding",
          "every valid routing command with any single byte replaced by any value",
          "each corrupted command is decoded",
          "decoding returns an error or a command that encodes back to the same bytes");

    for (const Message& message : AllCommands())
    {
        const auto valid = EncodeToVector(message);
        for (std::size_t position = 0U; position < valid.size(); ++position)
        {
            for (std::uint32_t value = 0U; value <= 0xFFU; ++value)
            {
                auto corrupted = valid;
                corrupted[position] = static_cast<std::uint8_t>(value);
                ExpectRejectedOrCanonical(corrupted);
            }
        }
    }
}

TEST(RoutingControlProtocolTest, DecodeHandlesRandomInput)
{
    Trace("RoutingControlProtocol.CR_RoutingControlProtocol_RobustDecoding",
          "random byte sequences of 0 to kMaxMessageSize + 2 bytes, half of them with a valid version, opcode and "
          "reserved byte",
          "each sequence is decoded",
          "decoding returns an error or a command that encodes back to the same bytes");

    std::mt19937 generator{0x5EEDU};
    std::uniform_int_distribution<std::uint32_t> byte{0U, 0xFFU};
    std::uniform_int_distribution<std::size_t> length{0U, kMaxMessageSize + 2U};
    std::uniform_int_distribution<std::uint32_t> opcode{1U, 9U};
    for (std::uint32_t iteration = 0U; iteration < 20000U; ++iteration)
    {
        std::vector<std::uint8_t> data(length(generator));
        for (auto& element : data)
        {
            element = static_cast<std::uint8_t>(byte(generator));
        }
        if (((iteration % 2U) == 0U) && (data.size() >= kHeaderSize))
        {
            data[detail::kProtocolVersionOffset] = kProtocolVersion;
            data[detail::kOpcodeOffset] = static_cast<std::uint8_t>(opcode(generator));
            data[detail::kReservedOffset] = 0U;
        }
        ExpectRejectedOrCanonical(data);
    }
}

}  // namespace
}  // namespace score::someip::routing_control_protocol
