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
#ifndef SCORE_SERIALIZATION_SOMEIP_SERIALIZATION_HELPERS_H
#define SCORE_SERIALIZATION_SOMEIP_SERIALIZATION_HELPERS_H

#include <score/span.hpp>

#include <cstdint>

namespace score::serialization::someip
{

/// \brief Big-endian (SOME/IP network byte order) read/write helpers shared by every serializer.
///
/// Each scalar writer expects out to span exactly the field width; each scalar reader expects in to
/// span exactly the field width. The array helpers cover the length-prefixed element block: callers
/// write the uint32 length prefix separately and use WriteFloats/ReadFloats for the elements.

void WriteUint16(std::uint16_t value, score::cpp::span<std::uint8_t> out) noexcept;
void WriteUint32(std::uint32_t value, score::cpp::span<std::uint8_t> out) noexcept;
void WriteFloat(float value, score::cpp::span<std::uint8_t> out) noexcept;

std::uint16_t ReadUint16(score::cpp::span<const std::uint8_t> in) noexcept;
std::uint32_t ReadUint32(score::cpp::span<const std::uint8_t> in) noexcept;
float ReadFloat(score::cpp::span<const std::uint8_t> in) noexcept;

/// \brief Writes values.size() big-endian floats consecutively into out.
///
/// out must be large enough to hold values.size() * sizeof(float) bytes.
void WriteFloats(score::cpp::span<const float> values, score::cpp::span<std::uint8_t> out) noexcept;

/// \brief Reads out.size() big-endian floats consecutively from in.
///
/// in must hold at least out.size() * sizeof(float) bytes.
void ReadFloats(score::cpp::span<const std::uint8_t> in, score::cpp::span<float> out) noexcept;

}  // namespace score::serialization::someip

#endif  // SCORE_SERIALIZATION_SOMEIP_SERIALIZATION_HELPERS_H
