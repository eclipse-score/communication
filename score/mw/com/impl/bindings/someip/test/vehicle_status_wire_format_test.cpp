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
#include "score/mw/com/impl/bindings/someip/test/vehicle_status.h"

#include "score/mw/com/impl/bindings/someip/vector_serialization_sink.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using someip::test::VehicleStatus;
using someip::test::WheelSpeeds;

constexpr VehicleStatus kSample{0x11223344U, 0x0064U, 0x03U, WheelSpeeds{1U, 2U, 3U, 4U}};

/// \brief The wire representation is what the serializer produced, byte for byte.
constexpr std::array<std::byte, 15U> kExpectedWireForm{std::byte{0x11},
                                                       std::byte{0x22},
                                                       std::byte{0x33},
                                                       std::byte{0x44},  // timestamp_ms, big endian
                                                       std::byte{0x00},
                                                       std::byte{0x64},  // speed_kmh, big endian
                                                       std::byte{0x03},  // gear
                                                       std::byte{0x00},
                                                       std::byte{0x01},  // wheel_speeds.front_left
                                                       std::byte{0x00},
                                                       std::byte{0x02},  // wheel_speeds.front_right
                                                       std::byte{0x00},
                                                       std::byte{0x03},  // wheel_speeds.rear_left
                                                       std::byte{0x00},
                                                       std::byte{0x04}};  // wheel_speeds.rear_right

std::vector<std::byte> SerializeSample(const VehicleStatus& sample)
{
    std::vector<std::byte> buffer{};
    someip::VectorSerializationSink sink{buffer, SampleWireFormat<VehicleStatus>::kMaxSerializedSize};
    EXPECT_TRUE(SampleWireFormat<VehicleStatus>::Serialize(sample, sink));
    EXPECT_FALSE(sink.HasWriteFailed());
    return buffer;
}

/// \brief The in-memory layout carries a padding byte the wire representation must not contain. If this ever stops
///        holding, the test below no longer demonstrates what it claims to.
TEST(VehicleStatusWireFormatTest, TheInMemoryLayoutIsLargerThanTheWireRepresentation)
{
    static_assert(sizeof(VehicleStatus) == 16U, "Expected one padding byte before wheel_speeds.");
    EXPECT_EQ(SampleWireFormat<VehicleStatus>::kMaxSerializedSize, 15U);
    EXPECT_LT(SampleWireFormat<VehicleStatus>::kMaxSerializedSize, sizeof(VehicleStatus));
}

TEST(VehicleStatusWireFormatTest, SerializesToTheExpectedBytes)
{
    const auto buffer = SerializeSample(kSample);

    ASSERT_EQ(buffer.size(), kExpectedWireForm.size());
    EXPECT_TRUE(std::equal(buffer.cbegin(), buffer.cend(), kExpectedWireForm.cbegin()));
}

/// \brief Scalars go on the wire in network byte order, which differs from the object representation on a little
///        endian host. Comparing against the raw bytes of the sample proves the serializer is doing the work.
TEST(VehicleStatusWireFormatTest, DiffersFromTheRawObjectRepresentation)
{
    const auto buffer = SerializeSample(kSample);

    std::array<std::byte, sizeof(VehicleStatus)> raw{};
    std::memcpy(raw.data(), &kSample, sizeof(VehicleStatus));

    ASSERT_NE(buffer.size(), raw.size());
    EXPECT_FALSE(std::equal(buffer.cbegin(), buffer.cend(), raw.cbegin()));
}

TEST(VehicleStatusWireFormatTest, SerializationIsDeterministicRegardlessOfPaddingContent)
{
    VehicleStatus first{};
    VehicleStatus second{};
    // Fill both objects, including any padding, with different byte patterns before assigning the same values.
    std::memset(&first, 0x00, sizeof(VehicleStatus));
    std::memset(&second, 0xFF, sizeof(VehicleStatus));
    first = kSample;
    second = kSample;

    EXPECT_EQ(SerializeSample(first), SerializeSample(second));
}

/// \brief A nested member must be written through the very same writer, otherwise its length/alignment state would
///        be lost. Observable here as the nested struct contributing its bytes at the correct offset.
TEST(VehicleStatusWireFormatTest, NestedMemberIsWrittenThroughTheSameWriter)
{
    const auto buffer = SerializeSample(kSample);

    ASSERT_EQ(buffer.size(), 15U);
    // wheel_speeds starts right after gear, i.e. at offset 7 with no padding in between.
    EXPECT_EQ(buffer[6U], std::byte{0x03});
    EXPECT_EQ(buffer[7U], std::byte{0x00});
    EXPECT_EQ(buffer[8U], std::byte{0x01});
}

}  // namespace
}  // namespace score::mw::com::impl
