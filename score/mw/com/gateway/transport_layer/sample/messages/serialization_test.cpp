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
#include "score/mw/com/gateway/transport_layer/sample/messages/serialization.h"

#include "score/mw/com/gateway/transport_layer/transport.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace score::mw::com::gateway
{
namespace
{

TEST(SerializationTest, SerializeAndDeserializeTrivialTypeProducesOriginalValue)
{
    const std::uint32_t original = 0xDEADBEEFU;
    std::array<std::byte, sizeof(original)> buf{};

    auto written = Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size()));
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(written.value(), sizeof(original));

    std::uint32_t out{};
    auto consumed = Deserialize(out, score::cpp::span<const std::byte>(buf.data(), buf.size()));
    ASSERT_TRUE(consumed.has_value());
    EXPECT_EQ(out, 0xDEADBEEFU);
}

TEST(SerializationTest, TrivialTypeSerializeAndDeserializeFailOnTooSmallBuffer)
{
    const std::uint32_t value = 42U;
    std::array<std::byte, 2> buf{};

    EXPECT_FALSE(Serialize(value, score::cpp::span<std::byte>(buf.data(), buf.size())).has_value());

    std::uint32_t out{};
    EXPECT_FALSE(Deserialize(out, score::cpp::span<const std::byte>(buf.data(), buf.size())).has_value());
}

TEST(SerializationTest, SerializeAndDeserializeStringProducesOriginalValue)
{
    const std::string original = "hello";
    constexpr std::size_t kExpectedSize = 5U + 1U;  // string length + null terminator
    std::array<std::byte, 64> buf{};

    auto written = Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size()));
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(written.value(), kExpectedSize);
    EXPECT_EQ(buf[original.size()], std::byte{0});

    std::string out;
    auto consumed = Deserialize(out, score::cpp::span<const std::byte>(buf.data(), written.value()));
    ASSERT_TRUE(consumed.has_value());
    EXPECT_EQ(out, "hello");
}

TEST(SerializationTest, StringSerializeFailsOnTooSmallBuffer)
{
    const std::string original = "hello";
    std::array<std::byte, 3> buf{};

    EXPECT_FALSE(Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size())).has_value());
}

TEST(SerializationTest, StringDeserializeFailsWithoutNullTerminator)
{
    std::array<std::byte, 4> buf{};
    std::memset(buf.data(), 'A', buf.size());

    std::string out;
    EXPECT_FALSE(Deserialize(out, score::cpp::span<const std::byte>(buf.data(), buf.size())).has_value());
}

TEST(SerializationTest, SerializeAndDeserializeVectorOfTrivialsProducesOriginalValues)
{
    const std::vector<std::uint16_t> original = {10U, 20U, 30U};
    std::array<std::byte, 64> buf{};

    constexpr std::size_t kNumElements = 3U;
    constexpr std::size_t kVectorHeaderSize = sizeof(std::uint16_t);
    constexpr std::size_t kExpectedSize = kVectorHeaderSize + kNumElements * sizeof(std::uint16_t);

    auto written = Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size()));
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(written.value(), kExpectedSize);

    std::vector<std::uint16_t> out;
    auto consumed = Deserialize(out, score::cpp::span<const std::byte>(buf.data(), written.value()));
    ASSERT_TRUE(consumed.has_value());
    EXPECT_EQ(out, original);
}

TEST(SerializationTest, EmptyVectorSerializesToJustTheElementCountHeader)
{
    const std::vector<std::uint32_t> original;
    std::array<std::byte, 64> buf{};

    auto written = Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size()));
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(written.value(), sizeof(std::uint16_t));

    std::vector<std::uint32_t> out;
    auto consumed = Deserialize(out, score::cpp::span<const std::byte>(buf.data(), written.value()));
    ASSERT_TRUE(consumed.has_value());
    EXPECT_TRUE(out.empty());
}

TEST(SerializationTest, SerializeAndDeserializeVectorOfStringsProducesOriginalValues)
{
    const std::vector<std::string> original = {"foo", "bar", "baz"};
    std::array<std::byte, 128> buf{};

    auto written = Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size()));
    ASSERT_TRUE(written.has_value());

    std::vector<std::string> out;
    auto consumed = Deserialize(out, score::cpp::span<const std::byte>(buf.data(), written.value()));
    ASSERT_TRUE(consumed.has_value());
    EXPECT_EQ(out, original);
}

TEST(SerializationTest, SerializeAndDeserializeVectorOfEventInfoProducesOriginalValues)
{
    // Expected values captured as independent std::strings, not string_views --
    // so the final comparison can't accidentally succeed by reading through a
    // stale/dangling view into memory we're about to free.
    const std::string kExpectedSpeedName = "SpeedEvent_LongEnoughToDefeatSSO_AAAAAAAAAA";
    const std::string kExpectedBrakeName = "BrakeEvent_LongEnoughToDefeatSSO_BBBBBBBBBB";
    const impl::DataTypeMetaInfo kExpectedSpeedMeta{64U, 8U};
    const impl::DataTypeMetaInfo kExpectedBrakeMeta{32U, 4U};

    std::array<std::byte, 256> buf{};
    score::Result<uint32_t> written;
    score::Result<uint32_t> consumed;
    std::vector<impl::EventInfo> out;

    {
        // Heap-allocate the source strings explicitly (not literals, not SSO-sized)
        // so their backing memory is a real, freeable heap block, and so we control
        // exactly when it's destroyed.
        auto speed_name = std::make_unique<std::string>(kExpectedSpeedName);
        auto brake_name = std::make_unique<std::string>(kExpectedBrakeName);

        std::vector<impl::EventInfo> original = {{std::string_view(*speed_name), kExpectedSpeedMeta},
                                                 {std::string_view(*brake_name), kExpectedBrakeMeta}};

        written = Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size()));
        ASSERT_TRUE(written.has_value());

        // original's string_views are only needed until Serialize() has copied
        // their bytes into buf -- drop the vector now.
        original.clear();
        original.shrink_to_fit();

        // Poison and free the actual string backing memory original's
        // string_views pointed at, BEFORE Deserialize is even called.
        std::memset(speed_name->data(), 0xCD, speed_name->size());
        std::memset(brake_name->data(), 0xCD, brake_name->size());
        speed_name.reset();
        brake_name.reset();

        // Nudge the allocator into reusing those freed blocks with different
        // content, so an accidental dependency on the old memory is more likely
        // to surface as a wrong value instead of coincidentally-correct leftovers.
        std::vector<std::unique_ptr<std::string>> heap_pressure;
        heap_pressure.reserve(64);
        for (int i = 0; i < 64; ++i)
        {
            heap_pressure.push_back(std::make_unique<std::string>(kExpectedSpeedName.size(), 'X'));
        }

        // Deserialize now runs with the original source strings already gone --
        // out[i].name can only be valid if it was built from buf, not from them.
        consumed = Deserialize(out, score::cpp::span<const std::byte>(buf.data(), written.value()));
    }

    ASSERT_TRUE(consumed.has_value());
    ASSERT_EQ(out.size(), 2U);
    EXPECT_EQ(out[0].name, kExpectedSpeedName);
    EXPECT_EQ(out[1].name, kExpectedBrakeName);
    EXPECT_EQ(out[0].data_type_meta_info.size, kExpectedSpeedMeta.size);
    EXPECT_EQ(out[0].data_type_meta_info.alignment, kExpectedSpeedMeta.alignment);
    EXPECT_EQ(out[1].data_type_meta_info.size, kExpectedBrakeMeta.size);
    EXPECT_EQ(out[1].data_type_meta_info.alignment, kExpectedBrakeMeta.alignment);
}

TEST(SerializationTest, VectorSerializeAndDeserializeFailWhenBufferTooSmallForHeader)
{
    const std::vector<std::uint32_t> original = {1U};
    std::array<std::byte, 1> buf{};

    EXPECT_FALSE(Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size())).has_value());

    std::vector<std::uint32_t> out;
    EXPECT_FALSE(Deserialize(out, score::cpp::span<const std::byte>(buf.data(), buf.size())).has_value());
}

TEST(SerializationTest, SerializeAndDeserializeNonTrivialStructProducesOriginalMembers)
{
    impl::EventInfo original{"SpeedEvent", impl::DataTypeMetaInfo{64U, 8U}};
    std::array<std::byte, 128> buf{};

    auto written = Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size()));
    ASSERT_TRUE(written.has_value());

    impl::EventInfo out;
    auto consumed = Deserialize(out, score::cpp::span<const std::byte>(buf.data(), written.value()));
    ASSERT_TRUE(consumed.has_value());
    EXPECT_EQ(out.name, "SpeedEvent");
    EXPECT_EQ(out.data_type_meta_info.size, original.data_type_meta_info.size);
}

TEST(SerializationTest, ComputeSerializedSizeReturnsCorrectValuesPerTypeCategory)
{
    constexpr std::size_t kVectorHeaderSize = sizeof(std::uint16_t);
    constexpr std::size_t kNullTerminatorSize = 1U;

    EXPECT_EQ(ComputeSerializedSize(std::uint32_t{0}), sizeof(std::uint32_t));
    EXPECT_EQ(ComputeSerializedSize(std::uint8_t{0}), sizeof(std::uint8_t));

    const std::string test_str = "test";
    EXPECT_EQ(ComputeSerializedSize(test_str), test_str.size() + kNullTerminatorSize);
    EXPECT_EQ(ComputeSerializedSize(std::string{}), kNullTerminatorSize);

    const std::vector<std::uint32_t> vec = {1U, 2U, 3U};
    EXPECT_EQ(ComputeSerializedSize(vec), kVectorHeaderSize + vec.size() * sizeof(std::uint32_t));

    const std::string ab = "ab";
    const std::string cde = "cde";
    const std::vector<std::string> str_vec = {ab, cde};
    const std::size_t kExpectedStrVecSize =
        kVectorHeaderSize + (ab.size() + kNullTerminatorSize) + (cde.size() + kNullTerminatorSize);
    EXPECT_EQ(ComputeSerializedSize(str_vec), kExpectedStrVecSize);

    EXPECT_EQ(ComputeSerializedSize(std::vector<std::uint32_t>{}), kVectorHeaderSize);
}

TEST(SerializationTest, ComputeSerializedSizeMatchesBytesWrittenBySerialize)
{
    std::array<std::byte, 512> buf{};

    const std::uint32_t trivial = 42U;
    const std::string str = "gateway_test";
    const std::vector<std::uint32_t> vec = {1U, 2U, 3U, 4U, 5U};
    impl::EventInfo config{"SpeedEvent", impl::DataTypeMetaInfo{64U, 8U}};
    std::vector<impl::EventInfo> configs = {
        {"SpeedEvent", impl::DataTypeMetaInfo{64U, 8U}},
        {"BrakeField", impl::DataTypeMetaInfo{32U, 4U}},
    };

    auto w1 = Serialize(trivial, score::cpp::span<std::byte>(buf.data(), buf.size()));
    EXPECT_EQ(ComputeSerializedSize(trivial), w1.value());

    auto w2 = Serialize(str, score::cpp::span<std::byte>(buf.data(), buf.size()));
    EXPECT_EQ(ComputeSerializedSize(str), w2.value());

    auto w3 = Serialize(vec, score::cpp::span<std::byte>(buf.data(), buf.size()));
    EXPECT_EQ(ComputeSerializedSize(vec), w3.value());

    auto w4 = Serialize(config, score::cpp::span<std::byte>(buf.data(), buf.size()));
    EXPECT_EQ(ComputeSerializedSize(config), w4.value());

    auto w5 = Serialize(configs, score::cpp::span<std::byte>(buf.data(), buf.size()));
    EXPECT_EQ(ComputeSerializedSize(configs), w5.value());
}

TEST(SerializationTest, SerializeSucceedsIntoExactSizeBufferAndFailsWithOneByteLess)
{
    const std::string value = "exact";
    const auto size = ComputeSerializedSize(value);

    std::vector<std::byte> exact_buf(size);
    EXPECT_TRUE(Serialize(value, score::cpp::span<std::byte>(exact_buf.data(), exact_buf.size())).has_value());

    std::vector<std::byte> small_buf(size - 1U);
    EXPECT_FALSE(Serialize(value, score::cpp::span<std::byte>(small_buf.data(), small_buf.size())).has_value());
}

TEST(SerializationTest, SerializeStringFailsIfTargetBufferTooSmall)
{
    // Given a string and a buffer that is smaller than the string's size
    const std::string value = "HelloWorld";
    const auto size = ComputeSerializedSize(value);

    std::vector<std::byte> small_buf(size - 1U);
    // when trying to serialize the string
    const auto result = Serialize(value, score::cpp::span<std::byte>(small_buf.data(), small_buf.size()));
    // then the result should not have a value and the error code should be kBufferTooSmall
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MessageErrorc::kBufferTooSmall);
}

TEST(SerializationTest, DeserializeStringFailsIfSourceBufferIsNotNullTerminated)
{
    // Given a buffer that is smaller than the expected string size and the string is copied into that buffer
    const std::string expected = "HelloWorld";
    const auto size = ComputeSerializedSize(expected);

    std::vector<std::byte> small_buf(size - 1U);
    std::memcpy(small_buf.data(), expected.data(), small_buf.size());

    // when trying to deserialize into a string, which is not null-terminated due to the small buffer
    std::string out;
    const auto result = Deserialize(out, score::cpp::span<const std::byte>(small_buf.data(), small_buf.size()));
    // then the result should not have a value and the error code should be kPayloadInvalid
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MessageErrorc::kPayloadInvalid);
}

TEST(SerializationTest, DeserializeStringFailsIfSourceBufferIsTooSmall)
{
    // Given a buffer that is smaller than the expected string size and the string is copied into that buffer
    const std::string expected = "HelloWorld";
    const auto size = ComputeSerializedSize(expected);
    std::vector<std::byte> small_buf(size - 1U);

    // when trying to deserialize into a string
    std::string out;
    const auto result = Deserialize(out, score::cpp::span<const std::byte>(small_buf.data(), small_buf.size()));
    // then the result should not have a value and the error code should be kPayloadInvalid
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MessageErrorc::kPayloadInvalid);
}

TEST(SerializationTest, SerializeVectorExceedingUint16MaxSizeReturnsError)
{
    // Given a vector with a size that exceeds the maximum representable by uint16_t
    const std::vector<std::uint32_t> original(1U + std::numeric_limits<std::uint16_t>::max(), 0U);
    std::array<std::byte, 1> buf{};
    // when trying to serialize the vector
    const auto serialize_result = Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size()));
    // then the result should have the error code kBufferTooSmall due to the size limit of input vectors
    EXPECT_FALSE(serialize_result.has_value());
    EXPECT_EQ(serialize_result.error(), MessageErrorc::kBufferTooSmall);
}

TEST(SerializationTest, SerializeVectorFailsIfNotEnoughSpaceForSubElement)
{
    // Given a vector with one element and a buffer that is too small to hold the element
    const std::vector<std::uint32_t> original = {42U};
    std::array<std::byte, sizeof(std::uint16_t) + sizeof(std::uint32_t) - 1U> buf{};

    // when trying to serialize the vector
    const auto serialize_result = Serialize(original, score::cpp::span<std::byte>(buf.data(), buf.size()));
    // then the result should have the error code kBufferTooSmall due to insufficient space for the element
    EXPECT_FALSE(serialize_result.has_value());
    EXPECT_EQ(serialize_result.error(), MessageErrorc::kBufferTooSmall);
}

TEST(SerializationTest, DeserializeVectorFailsIfNotEnoughSpaceForSubElement)
{
    // Given a buffer that encodes a vector of 1 uint32_t element, but does not have enough bytes for the element itself
    constexpr std::uint16_t num_elements = 1U;
    // Buffer holds the 2-byte element count header, but only 3 bytes for the element
    std::array<std::byte, sizeof(std::uint16_t) + sizeof(std::uint32_t) - 1U> buf{};
    std::memcpy(buf.data(), &num_elements, sizeof(num_elements));

    // when trying to deserialize the vector
    std::vector<std::uint32_t> out;
    const auto result = Deserialize(out, score::cpp::span<const std::byte>(buf.data(), buf.size()));

    // then the result should not have a value and the error code should be kPayloadInvalid
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MessageErrorc::kPayloadInvalid);
}

TEST(SerializationTest, SerializeNonTrivialCopyableTypeFailsIfBufferTooSmall)
{
    // Given a non-trivial copyable type and a buffer that is too small to hold its serialized form
    impl::EventInfo config{"SpeedEvent", impl::DataTypeMetaInfo{64U, 8U}};
    std::array<std::byte, 1> buf{};

    // when trying to serialize the non-trivial type
    const auto serialize_result = Serialize(config, score::cpp::span<std::byte>(buf.data(), buf.size()));
    // then the result should have the error code kBufferTooSmall due to insufficient space for the serialized struct
    EXPECT_FALSE(serialize_result.has_value());
    EXPECT_EQ(serialize_result.error(), MessageErrorc::kBufferTooSmall);
}

TEST(SerializationTest, DeserializeNonTrivialCopyableTypeFailsIfBufferTooSmall)
{
    // Given a buffer that is too small to hold the serialized form of a non-trivial copyable type
    std::array<std::byte, 1> buf{};

    // when trying to deserialize into a non-trivial type
    impl::EventInfo out;
    const auto deserialize_result = Deserialize(out, score::cpp::span<const std::byte>(buf.data(), buf.size()));
    // then the result should have the error code kPayloadInvalid due to insufficient bytes for deserialization
    EXPECT_FALSE(deserialize_result.has_value());
    EXPECT_EQ(deserialize_result.error(), MessageErrorc::kPayloadInvalid);
}
}  // namespace
}  // namespace score::mw::com::gateway
