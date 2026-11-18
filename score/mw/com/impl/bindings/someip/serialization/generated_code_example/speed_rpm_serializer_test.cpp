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
#include "score/mw/com/impl/bindings/someip/serialization/generated_code_example/speed_rpm_serializer.h"

#include "score/mw/com/impl/bindings/someip/serialization/generated_code_example/speed_rpm.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace score::serialization::someip
{
namespace
{

SpeedRpm MakeSpeedRpmWithThreeSpeedSamples()
{
    SpeedRpm sample{};
    sample.speed_kmh = 150.75F;
    sample.rpm = 5000U;
    sample.speed_samples = {100.0F, 125.0F, 150.75F};
    return sample;
}

SpeedRpm MakeSpeedRpmWithMaxSpeedSamples()
{
    SpeedRpm sample{};
    sample.speed_kmh = 1.0F;
    sample.rpm = 2U;
    for (std::size_t i = 0U; i < SpeedRpm::kMaxSpeedSamples; ++i)
    {
        sample.speed_samples.push_back(static_cast<float>(i));
    }
    return sample;
}

TEST(SpeedRpmSerializer, RoundTripWithSamplesRestoresValues)
{
    // Given a serializer and a SpeedRpm with three speed samples
    const SpeedRpmSerializer serializer{};
    const SpeedRpm original = MakeSpeedRpmWithThreeSpeedSamples();

    // When serializing the sample and deserializing the result into another SpeedRpm
    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    ASSERT_TRUE(serializer.Serialize(&original, buffer).has_value());

    SpeedRpm decoded{};
    ASSERT_TRUE(serializer.Deserialize(buffer, &decoded).has_value());

    // Then all values of the original are restored
    EXPECT_FLOAT_EQ(decoded.speed_kmh, original.speed_kmh);
    EXPECT_EQ(decoded.rpm, original.rpm);
    EXPECT_EQ(decoded.speed_samples, original.speed_samples);
}

TEST(SpeedRpmSerializer, SerializesEmptySampleArray)
{
    // Given a serializer and a SpeedRpm without speed samples
    const SpeedRpmSerializer serializer{};

    SpeedRpm original{};
    original.speed_kmh = 100.5F;
    original.rpm = 3500U;

    // When serializing the sample and deserializing the result into another SpeedRpm
    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    ASSERT_TRUE(serializer.Serialize(&original, buffer).has_value());

    SpeedRpm decoded{};
    ASSERT_TRUE(serializer.Deserialize(buffer, &decoded).has_value());

    // Then the decoded SpeedRpm has no speed samples
    EXPECT_TRUE(decoded.speed_samples.empty());
}

TEST(SpeedRpmSerializer, WritesBigEndianWireLayout)
{
    // Given a serializer and a SpeedRpm with one speed sample
    const SpeedRpmSerializer serializer{};

    SpeedRpm original{};
    original.speed_kmh = 100.5F;  // 0x42C90000
    original.rpm = 3500U;         // 0x0DAC
    original.speed_samples = {50.0F};

    // When serializing the sample into a buffer that is larger than the encoding
    std::array<std::uint8_t, 16U> buffer{};
    ASSERT_TRUE(serializer.Serialize(&original, buffer).has_value());

    // Then the values are written in big-endian byte order, followed by the unused tail of the buffer
    const std::array<std::uint8_t, 16U> expected{
        0x42U,
        0xC9U,
        0x00U,
        0x00U,  // speed_kmh
        0x0DU,
        0xACU,  // rpm
        0x00U,
        0x00U,
        0x00U,
        0x04U,  // length of the sample data in bytes (one float)
        0x42U,
        0x48U,
        0x00U,
        0x00U,  // sample[0] = 50.0F
        0x00U,
        0x00U,  // unused tail of buffer
    };
    EXPECT_EQ(buffer, expected);
}

TEST(SpeedRpmSerializer, SerializedSizeIsFixedHeaderPlusSpeedSamples)
{
    // Given a serializer and a SpeedRpm with three speed samples
    const SpeedRpmSerializer serializer{};
    const SpeedRpm original = MakeSpeedRpmWithThreeSpeedSamples();

    // When serializing the sample into a buffer of the maximum serialized size
    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    const auto written = serializer.Serialize(&original, buffer);
    ASSERT_TRUE(written.has_value());

    // Then the fixed 10 bytes (speed_kmh, rpm, length of the samples) and the speed samples are written
    EXPECT_EQ(written.value(), 10U + original.speed_samples.size() * sizeof(float));
}

TEST(SpeedRpmSerializer, SerializeRejectsNullSample)
{
    // Given a serializer and a buffer of the maximum serialized size
    const SpeedRpmSerializer serializer{};
    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());

    // When serializing a null sample
    const auto result = serializer.Serialize(nullptr, buffer);

    // Then the result is an invalid argument error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kInvalidArgument));
}

TEST(SpeedRpmSerializer, SerializeRejectsTooSmallBuffer)
{
    // Given a serializer, a SpeedRpm with two speed samples and a buffer that has no room for them
    const SpeedRpmSerializer serializer{};

    SpeedRpm original{};
    original.speed_samples = {1.0F, 2.0F};

    std::array<std::uint8_t, 12U> buffer{};  // header only, no room for samples

    // When serializing the sample into the buffer
    const auto result = serializer.Serialize(&original, buffer);

    // Then the result is a buffer too small error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kBufferTooSmall));
}

TEST(SpeedRpmSerializer, DeserializeRejectsTruncatedHeader)
{
    // Given a serializer and a buffer that is shorter than the fixed 10 bytes of the encoding
    const SpeedRpmSerializer serializer{};

    std::array<std::uint8_t, 8U> buffer{};
    SpeedRpm decoded{};

    // When deserializing the buffer
    const auto result = serializer.Deserialize(buffer, &decoded);

    // Then the result is a deserialization failed error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kDeserializationFailed));
}

TEST(SpeedRpmSerializer, DeserializeRejectsLengthExceedingBuffer)
{
    // Given a serializer and a buffer whose length field announces more bytes than the buffer holds
    const SpeedRpmSerializer serializer{};

    std::array<std::uint8_t, 12U> buffer{};
    buffer[9] = 0x08U;  // declares two floats, but only two bytes follow the length field

    SpeedRpm decoded{};

    // When deserializing the buffer
    const auto result = serializer.Deserialize(buffer, &decoded);

    // Then the result is a deserialization failed error
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kDeserializationFailed));
}

TEST(SpeedRpmSerializer, DeserializeRejectsMoreSamplesThanCapacity)
{
    // Given a serializer and a buffer whose length field announces one speed sample more than a SpeedRpm can hold
    const SpeedRpmSerializer serializer{};

    constexpr std::size_t kHeaderSize = 10U;
    std::vector<std::uint8_t> buffer(kHeaderSize + ((SpeedRpm::kMaxSpeedSamples + 1U) * sizeof(float)));
    buffer[8] = 0x01U;
    buffer[9] = 0x94U;  // 404 bytes = 101 floats, one more than fits into speed_samples

    SpeedRpm decoded{};

    // When deserializing the buffer
    const auto result = serializer.Deserialize(buffer, &decoded);

    // Then the result is a deserialization failed error that reports the length of speed_samples as out of range
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kDeserializationFailed));
    EXPECT_EQ(result.error().UserMessage(), "speed_samples length out of range");
}

TEST(SpeedRpmSerializer, DeserializeReplacesExistingSamples)
{
    // Given a serializer, a buffer with the encoding of a SpeedRpm with two speed samples
    // and a SpeedRpm that already holds three other speed samples
    const SpeedRpmSerializer serializer{};

    SpeedRpm original{};
    original.speed_samples = {1.0F, 2.0F};

    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    ASSERT_TRUE(serializer.Serialize(&original, buffer).has_value());

    SpeedRpm decoded{};
    decoded.speed_samples = {9.0F, 9.0F, 9.0F};

    // When deserializing the buffer into that SpeedRpm
    ASSERT_TRUE(serializer.Deserialize(buffer, &decoded).has_value());

    // Then the existing speed samples are replaced by the decoded ones
    EXPECT_EQ(decoded.speed_samples, original.speed_samples);
}

TEST(SpeedRpmSerializer, DeserializeLeavesSampleUntouchedOnFailure)
{
    // Given a serializer, a buffer that is shorter than the fixed 10 bytes of the encoding
    // and a SpeedRpm with values
    const SpeedRpmSerializer serializer{};

    std::array<std::uint8_t, 8U> truncated{};
    SpeedRpm decoded{};
    decoded.speed_kmh = 7.0F;
    decoded.rpm = 8U;
    decoded.speed_samples = {1.0F};

    // When deserializing the buffer into that SpeedRpm
    ASSERT_FALSE(serializer.Deserialize(truncated, &decoded).has_value());

    // Then the SpeedRpm keeps its values
    EXPECT_FLOAT_EQ(decoded.speed_kmh, 7.0F);
    EXPECT_EQ(decoded.rpm, 8U);
    EXPECT_EQ(decoded.speed_samples.size(), 1U);
}

TEST(SpeedRpmSerializer, FullSampleArraySerializesToMaxSerializedSize)
{
    // Given a serializer and a SpeedRpm with the maximum number of speed samples
    const SpeedRpmSerializer serializer{};
    const SpeedRpm original = MakeSpeedRpmWithMaxSpeedSamples();

    // When serializing the sample into a buffer of the maximum serialized size
    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    const auto written = serializer.Serialize(&original, buffer);
    ASSERT_TRUE(written.has_value());

    // Then the whole buffer is used
    EXPECT_EQ(written.value(), buffer.size());
}

TEST(SpeedRpmSerializer, RoundTripWithFullSampleArrayRestoresSpeedSamples)
{
    // Given a serializer and a SpeedRpm with the maximum number of speed samples
    const SpeedRpmSerializer serializer{};
    const SpeedRpm original = MakeSpeedRpmWithMaxSpeedSamples();

    // When serializing the sample and deserializing the result into another SpeedRpm
    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    ASSERT_TRUE(serializer.Serialize(&original, buffer).has_value());

    SpeedRpm decoded{};
    ASSERT_TRUE(serializer.Deserialize(buffer, &decoded).has_value());

    // Then all speed samples are restored
    EXPECT_EQ(decoded.speed_samples, original.speed_samples);
}

}  // namespace
}  // namespace score::serialization::someip
