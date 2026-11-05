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
#include "score/serialization/someip/generated_code_example/speed_rpm_serializer.h"

#include "score/serialization/someip/generated_code_example/speed_rpm.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace score::serialization::someip
{
namespace
{

TEST(SpeedRpmSerializer, RoundTripWithSamplesRestoresValues)
{
    const SpeedRpmSerializer serializer{};

    SpeedRpm original{};
    original.speed_kmh = 150.75F;
    original.rpm = 5000U;
    original.speed_samples = {100.0F, 125.0F, 150.75F};

    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    ASSERT_TRUE(serializer.Serialize(&original, buffer).has_value());

    SpeedRpm decoded{};
    ASSERT_TRUE(serializer.Deserialize(buffer, &decoded).has_value());

    EXPECT_FLOAT_EQ(decoded.speed_kmh, original.speed_kmh);
    EXPECT_EQ(decoded.rpm, original.rpm);
    EXPECT_EQ(decoded.speed_samples, original.speed_samples);
}

TEST(SpeedRpmSerializer, SerializesEmptySampleArray)
{
    const SpeedRpmSerializer serializer{};

    SpeedRpm original{};
    original.speed_kmh = 100.5F;
    original.rpm = 3500U;

    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    ASSERT_TRUE(serializer.Serialize(&original, buffer).has_value());

    SpeedRpm decoded{};
    ASSERT_TRUE(serializer.Deserialize(buffer, &decoded).has_value());
    EXPECT_TRUE(decoded.speed_samples.empty());
}

TEST(SpeedRpmSerializer, WritesBigEndianWireLayout)
{
    const SpeedRpmSerializer serializer{};

    SpeedRpm original{};
    original.speed_kmh = 100.5F;  // 0x42C90000
    original.rpm = 3500U;         // 0x0DAC
    original.speed_samples = {50.0F};

    std::array<std::uint8_t, 16U> buffer{};
    ASSERT_TRUE(serializer.Serialize(&original, buffer).has_value());

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
        0x01U,  // sample count
        0x42U,
        0x48U,
        0x00U,
        0x00U,  // sample[0] = 50.0F
        0x00U,
        0x00U,  // unused tail of buffer
    };
    EXPECT_EQ(buffer, expected);
}

TEST(SpeedRpmSerializer, SerializeRejectsNullSample)
{
    const SpeedRpmSerializer serializer{};

    std::vector<std::uint8_t> buffer(serializer.GetMaxSerializedSize().value());
    const auto result = serializer.Serialize(nullptr, buffer);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kInvalidArgument));
}

TEST(SpeedRpmSerializer, SerializeRejectsTooSmallBuffer)
{
    const SpeedRpmSerializer serializer{};

    SpeedRpm original{};
    original.speed_samples = {1.0F, 2.0F};

    std::array<std::uint8_t, 12U> buffer{};  // header only, no room for samples
    const auto result = serializer.Serialize(&original, buffer);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kBufferTooSmall));
}

TEST(SpeedRpmSerializer, DeserializeRejectsTruncatedHeader)
{
    const SpeedRpmSerializer serializer{};

    std::array<std::uint8_t, 8U> buffer{};
    SpeedRpm decoded{};
    const auto result = serializer.Deserialize(buffer, &decoded);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kDeserializationFailed));
}

TEST(SpeedRpmSerializer, DeserializeRejectsOutOfRangeSampleCount)
{
    const SpeedRpmSerializer serializer{};

    std::array<std::uint8_t, 12U> buffer{};
    buffer[6] = 0x00U;
    buffer[7] = 0x00U;
    buffer[8] = 0x00U;
    buffer[9] = 0x65U;  // 101 samples, exceeds the maximum

    SpeedRpm decoded{};
    const auto result = serializer.Deserialize(buffer, &decoded);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<score::result::ErrorCode>(SerializerErrc::kDeserializationFailed));
}

}  // namespace
}  // namespace score::serialization::someip
