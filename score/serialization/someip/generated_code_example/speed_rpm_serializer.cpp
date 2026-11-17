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
#include "score/serialization/someip/serialization_helpers.h"

#include <new>

namespace score::serialization::someip
{

score::Result<std::size_t> SpeedRpmSerializer::Serialize(const void* sample,
                                                  score::cpp::span<std::uint8_t> buffer) const noexcept
{
    if (sample == nullptr)
    {
        return MakeUnexpected(SerializerErrc::kInvalidArgument);
    }

    const auto& event = *static_cast<const SpeedRpm*>(sample);
    if (event.speed_samples.size() > kMaxSpeedSamples)
    {
        return MakeUnexpected(SerializerErrc::kInternalError, "speed_samples exceeds maximum length");
    }

    const std::size_t required = kFixedFieldsSize + (event.speed_samples.size() * sizeof(float));
    if (buffer.size() < required)
    {
        return MakeUnexpected(SerializerErrc::kBufferTooSmall);
    }

    WriteFloat(event.speed_kmh, buffer.subspan(0, sizeof(float)));
    WriteUint16(event.rpm, buffer.subspan(4, sizeof(std::uint16_t)));
    WriteUint32(static_cast<std::uint32_t>(event.speed_samples.size()), buffer.subspan(6, sizeof(std::uint32_t)));
    WriteFloats(event.speed_samples, buffer.subspan(kFixedFieldsSize));

    return required;
}

score::Result<void> SpeedRpmSerializer::Deserialize(score::cpp::span<const std::uint8_t> buffer,
                                                    void* sample) const noexcept
{
    if (sample == nullptr)
    {
        return MakeUnexpected(SerializerErrc::kInvalidArgument);
    }

    if (buffer.size() < kFixedFieldsSize)
    {
        return MakeUnexpected(SerializerErrc::kDeserializationFailed, "buffer smaller than fixed fields");
    }

    const std::uint32_t sample_count = ReadUint32(buffer.subspan(6, sizeof(std::uint32_t)));
    if (sample_count > kMaxSpeedSamples)
    {
        return MakeUnexpected(SerializerErrc::kDeserializationFailed, "speed_samples length out of range");
    }

    const std::size_t required = kFixedFieldsSize + (sample_count * sizeof(float));
    if (buffer.size() < required)
    {
        return MakeUnexpected(SerializerErrc::kDeserializationFailed, "buffer truncated");
    }

    auto& event = *static_cast<SpeedRpm*>(sample);
    event.speed_kmh = ReadFloat(buffer.subspan(0, sizeof(float)));
    event.rpm = ReadUint16(buffer.subspan(4, sizeof(std::uint16_t)));

    event.speed_samples.resize(sample_count);
    ReadFloats(buffer.subspan(kFixedFieldsSize), event.speed_samples);

    return {};
}

score::Result<std::size_t> SpeedRpmSerializer::GetMaxSerializedSize() const noexcept
{
    return kMaxSerializedSize;
}

}  // namespace score::serialization::someip

namespace
{
using score::serialization::someip::SpeedRpm;

// The exported symbol name must spell out the hashes as literals, so pin them to the datatype constants.
static_assert(SpeedRpm::kStructuralHash == 0x2c5f8d1aULL, "Symbol name does not match structural hash");
static_assert(SpeedRpm::kAttributeHash == 0x7e3a91c4ULL, "Symbol name does not match attribute hash");
}  // namespace

extern "C" {

// Every generated library exports one factory function per serializer variant, resolved by mw::com via
// dlsym. The symbol name encodes the structural hash of the datatype and the hash of the attributes
// fixed at compile time:
//   ISerializer* CreateSerializer_<structural-hash>_<attribute-hash>(<other-attributes>) noexcept;
// <other-attributes> are the attributes configured at runtime (none for this variant). The factory
// returns a heap-allocated ISerializer, or nullptr if construction fails. The caller owns it and
// releases it with DestroySerializer, so allocation and deallocation happen on the same side.
score::serialization::someip::ISerializer* CreateSerializer_2c5f8d1a_7e3a91c4() noexcept
{
    return new (std::nothrow) score::serialization::someip::SpeedRpmSerializer{};
}

void DestroySerializer(score::serialization::someip::ISerializer* serializer) noexcept
{
    delete serializer;
}

}  // extern "C"
