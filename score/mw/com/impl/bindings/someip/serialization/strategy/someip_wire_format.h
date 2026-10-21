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
#ifndef SCORE_SERIALIZATION_STRATEGIES_SOMEIP_WIRE_FORMAT_H
#define SCORE_SERIALIZATION_STRATEGIES_SOMEIP_WIRE_FORMAT_H

#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_layout.h"

#include "score/memory/endianness.h"
#include "score/serialization/serialize_iterator.h"

#include <score/utility.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace score
{
namespace serialization
{
namespace someip_detail
{

/// @brief Maps a wire byte order to the "swap needed" decision for the host endianness.
inline bool ReverseFor(const ByteOrder order)
{
    return memory::SwitchingBytesNecessary(order);
}

/// @brief Writes any trivially-copyable fixed-width scalar, reversing the byte order when requested (big-endian is
/// the SOME/IP default). The object representation is treated as raw bytes.
template <typename Container, typename T>
void WriteScalar(SerializeIterator<Container> iterator, const T& value, const bool reverse)
{
    static_assert(std::is_trivially_copyable<T>::value, "Serialized value must be TriviallyCopyable");
    constexpr auto size = sizeof(T);
    // Read the object representation directly as bytes (char-type access is permitted by the aliasing rules) and
    // write them straight to the destination in the configured order -- no intermediate buffer, no extra copy.
    // coverity[autosar_cpp14_a5_2_4_violation]
    const auto* const raw = reinterpret_cast<const std::uint8_t*>(&value);  // NOLINT(*-reinterpret-cast) justified
    for (std::size_t index{0U}; index < size; ++index)
    {
        const std::size_t source{reverse ? ((size - 1U) - index) : index};
        iterator = raw[source];  // SerializeIterator advances itself on assignment
    }
}

template <typename Container, typename T>
void ReadScalar(DeSerializeIterator<Container>& iterator, T& value, const bool reverse)
{
    static_assert(std::is_trivially_copyable<T>::value, "Deserialized value must be TriviallyCopyable");
    constexpr auto size = sizeof(T);
    // Write the incoming bytes straight into the object representation in the configured order -- no temporary.
    // coverity[autosar_cpp14_a5_2_4_violation]
    auto* const raw = reinterpret_cast<std::uint8_t*>(&value);  // NOLINT(*-reinterpret-cast) justified
    for (std::size_t index{0U}; index < size; ++index)
    {
        if (iterator == iterator.End())
        {
            // Truncated input: latch an error instead of dereferencing the exhausted iterator (which would abort).
            iterator = MakeUnexpected(SerializationError::kOutOfBoundsWrite);
            return;
        }
        const std::size_t destination{reverse ? ((size - 1U) - index) : index};
        raw[destination] = *iterator;
        score::cpp::ignore = ++iterator;
    }
}

template <typename Container>
void EmitAlignmentPadding(SerializeIterator<Container>& iterator, const std::uint16_t alignment_bits)
{
    const std::size_t align = static_cast<std::size_t>(alignment_bits) / kBitsPerByte;
    if (align > 1U)
    {
        const std::size_t pad = (align - (iterator.Offset() % align)) % align;
        for (std::size_t i{0U}; i < pad; ++i)
        {
            iterator = kPaddingByte;  // padding byte
        }
    }
}

template <typename Container>
void SkipAlignmentPadding(DeSerializeIterator<Container>& iterator, const std::uint16_t alignment_bits)
{
    const std::size_t align = static_cast<std::size_t>(alignment_bits) / kBitsPerByte;
    if (align > 1U)
    {
        const std::size_t pad = (align - (iterator.Offset() % align)) % align;
        for (std::size_t i{0U}; i < pad; ++i)
        {
            score::cpp::ignore = ++iterator;  // skip padding
        }
    }
}

/// @brief Writes a dynamic-member length prefix of configurable width (length_field_bytes) using the element's byte
/// order. Big-endian writes the most-significant byte first. No alignment padding is applied here; per SOME/IP,
/// padding goes *after* a variable-length element (see the array/string helpers), not before its length field.
template <typename Container>
void WriteLengthField(SerializeIterator<Container> iterator, const std::uint64_t length, const ElementLayout& layout)
{
    if (layout.length_field_bytes > kMaxLengthFieldBytes)
    {
        iterator = MakeUnexpected(SerializationError::kOutOfBoundsWrite);  // SOME/IP length field is at most 4 bytes
        return;
    }
    const bool big_endian = (layout.byte_order == ByteOrder::kBigEndian);
    for (std::uint8_t index{0U}; index < layout.length_field_bytes; ++index)
    {
        const std::uint8_t byte_index =
            big_endian ? static_cast<std::uint8_t>(layout.length_field_bytes - 1U - index) : index;
        iterator = static_cast<std::uint8_t>((length >> (kBitsPerByte * byte_index)) & kByteMask);
    }
}

template <typename Container>
std::uint64_t ReadLengthField(DeSerializeIterator<Container>& iterator, const ElementLayout& layout)
{
    if (layout.length_field_bytes > kMaxLengthFieldBytes)
    {
        iterator = MakeUnexpected(SerializationError::kOutOfBoundsWrite);  // SOME/IP length field is at most 4 bytes
        return 0U;
    }
    const bool    big_endian = (layout.byte_order == ByteOrder::kBigEndian);
    std::uint64_t length{0U};
    for (std::uint8_t index{0U}; index < layout.length_field_bytes; ++index)
    {
        if (iterator == iterator.End())
        {
            // Truncated input: latch an error instead of dereferencing the exhausted iterator (which would abort).
            iterator = MakeUnexpected(SerializationError::kOutOfBoundsWrite);
            return 0U;
        }
        const std::uint8_t byte_index =
            big_endian ? static_cast<std::uint8_t>(layout.length_field_bytes - 1U - index) : index;
        length |= static_cast<std::uint64_t>(*iterator) << (kBitsPerByte * byte_index);
        score::cpp::ignore = ++iterator;
    }
    return length;
}

}  // namespace someip_detail
}  // namespace serialization
}  // namespace score

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_WIRE_FORMAT_H
