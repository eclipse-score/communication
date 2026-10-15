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
#include "score/serialization/someip/serialization_helpers.h"

#include <cstddef>
#include <cstring>

namespace score::serialization::someip
{

void WriteUint16(const std::uint16_t value, score::cpp::span<std::uint8_t> out) noexcept
{
    out[0] = static_cast<std::uint8_t>((static_cast<unsigned int>(value) >> 8U) & 0xFFU);
    out[1] = static_cast<std::uint8_t>(static_cast<unsigned int>(value) & 0xFFU);
}

void WriteUint32(const std::uint32_t value, score::cpp::span<std::uint8_t> out) noexcept
{
    out[0] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
    out[1] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    out[2] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    out[3] = static_cast<std::uint8_t>(value & 0xFFU);
}

void WriteFloat(const float value, score::cpp::span<std::uint8_t> out) noexcept
{
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    WriteUint32(bits, out);
}

std::uint16_t ReadUint16(score::cpp::span<const std::uint8_t> in) noexcept
{
    return static_cast<std::uint16_t>((static_cast<unsigned int>(in[0]) << 8U) | static_cast<unsigned int>(in[1]));
}

std::uint32_t ReadUint32(score::cpp::span<const std::uint8_t> in) noexcept
{
    return (static_cast<std::uint32_t>(in[0]) << 24U) | (static_cast<std::uint32_t>(in[1]) << 16U) |
           (static_cast<std::uint32_t>(in[2]) << 8U) | static_cast<std::uint32_t>(in[3]);
}

float ReadFloat(score::cpp::span<const std::uint8_t> in) noexcept
{
    const std::uint32_t bits = ReadUint32(in);
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void WriteFloats(score::cpp::span<const float> values, score::cpp::span<std::uint8_t> out) noexcept
{
    std::size_t offset = 0U;
    for (const float value : values)
    {
        WriteFloat(value, out.subspan(offset, sizeof(float)));
        offset += sizeof(float);
    }
}

void ReadFloats(score::cpp::span<const std::uint8_t> in, score::cpp::span<float> out) noexcept
{
    std::size_t offset = 0U;
    for (float& value : out)
    {
        value = ReadFloat(in.subspan(offset, sizeof(float)));
        offset += sizeof(float);
    }
}

}  // namespace score::serialization::someip
