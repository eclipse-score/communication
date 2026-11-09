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
#ifndef SCORE_SERIALIZATION_STRATEGIES_SOMEIP_LAYOUT_H
#define SCORE_SERIALIZATION_STRATEGIES_SOMEIP_LAYOUT_H

#include "score/memory/endianness.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace score
{
namespace serialization
{

/// @brief Number of bits in a byte; used to convert between byte- and bit-oriented quantities.
constexpr std::size_t kBitsPerByte{8U};
/// @brief Mask selecting the low 8 bits of a value.
constexpr std::uint64_t kByteMask{0xFFU};
/// @brief SOME/IP boolean encodings: FALSE = 0x00, TRUE = 0x01.
constexpr std::uint8_t kBoolFalse{0x00U};
constexpr std::uint8_t kBoolTrue{0x01U};
/// @brief A SOME/IP boolean occupies a single byte on the wire.
constexpr std::size_t kBoolWireSize{1U};
/// @brief Value written for alignment padding. SOME/IP leaves the padding value undefined; this strategy uses 0x00.
constexpr std::uint8_t kPaddingByte{0x00U};
/// @brief Alignment boundaries SOME/IP permits behind variable-length data, in bits.
constexpr std::uint16_t kAlign8Bits{8U};
constexpr std::uint16_t kAlign16Bits{16U};
constexpr std::uint16_t kAlign32Bits{32U};
constexpr std::uint16_t kAlign64Bits{64U};
constexpr std::uint16_t kAlign128Bits{128U};
constexpr std::uint16_t kAlign256Bits{256U};
/// @brief SOME/IP defaults: 8-bit alignment (packed) and a 4-byte on-wire length prefix.
constexpr std::uint16_t kDefaultAlignmentBits{kAlign8Bits};
constexpr std::uint8_t kDefaultLengthFieldBytes{4U};
/// @brief SOME/IP permits only 0/1/2/4-byte length fields.
constexpr bool IsValidLengthFieldBytes(const std::uint8_t bytes)
{
    return (bytes == 0U) || (bytes == sizeof(std::uint8_t)) || (bytes == sizeof(std::uint16_t)) ||
           (bytes == sizeof(std::uint32_t));
}

namespace someip_detail
{
/// @brief Unwraps an enum to its underlying integral type; leaves non-enum types unchanged.
template <typename T, bool = std::is_enum<T>::value>
struct underlying_type
{
    using type = typename std::underlying_type<T>::type;
};
template <typename T>
struct underlying_type<T, false>
{
    using type = T;
};

/// @brief Matches every fixed-width scalar SOME/IP encodes in the configured byte order: signed and unsigned
/// integers, enums (any integral underlying type), and floating point (IEEE-754). bool is excluded; it is a single
/// byte and is handled separately.
template <typename T>
struct is_fixed_width_scalar
{
    using underlying = typename underlying_type<T>::type;
    static constexpr bool kFixedWidth =
        (((sizeof(T) == 1U) || (sizeof(T) == 2U)) || ((sizeof(T) == 4U) || (sizeof(T) == 8U)));
    static constexpr bool value = kFixedWidth && (!std::is_same<underlying, bool>::value) &&
                                  (std::is_integral<underlying>::value || std::is_floating_point<underlying>::value);
};
}  // namespace someip_detail

/// @brief Wire byte order for a scalar element. Reuses the memory library's endianness type instead of a parallel
/// enum; SOME/IP defaults to big-endian.
using ByteOrder = memory::Endianness;

/// @brief Per-element SOME/IP wire properties. Supplied out-of-band by the datatype's generated layout, so the user's
/// data type is never decorated. ElementLayout and SomeIpLayout form a tree: an element may point to a nested layout
/// describing its own sub-structure (a nested struct's fields, or an array's element type), enabling per-level
/// configuration for arbitrarily nested types. A null `nested` means that subtree is uniform and inherits.
struct SomeIpLayout;

struct ElementLayout
{
    ByteOrder byte_order{ByteOrder::kBigEndian};
    std::uint16_t alignment_bits{kDefaultAlignmentBits};  // 8/16/32/64/128/256; 8 == packed (no padding)
    std::uint8_t length_field_bytes{
        kDefaultLengthFieldBytes};  // width of the on-wire length prefix for dynamic members (string/vector)
    // Layout for this element's own sub-structure: a nested struct's fields, or an array's element type. Null means
    // that subtree uses the SOME/IP default ElementLayout.
    const SomeIpLayout* nested{nullptr};
};

/// @brief Layout of a struct (or an array's element type): a default applied to every element, plus optional per-field
/// overrides indexed by field order. A null per_field (the default) means the datatype is uniform.
struct SomeIpLayout
{
    ElementLayout default_{};
    const ElementLayout* per_field{nullptr};
    std::size_t per_field_count{0U};
};

namespace someip_detail
{

/// @brief Position in the layout tree while descending into nested elements. Shared by the strategy and GetMaxSize so
/// both resolve identical layouts. A null layout anywhere yields the SOME/IP default ElementLayout.
struct LayoutCursor
{
    const SomeIpLayout* struct_layout{nullptr};  // layout whose fields the next field step indexes
    ElementLayout leaf{};                        // layout of the current element
};

inline LayoutCursor RootCursor(const SomeIpLayout& layout)
{
    return {&layout, layout.default_};
}

inline ElementLayout ElementAt(const SomeIpLayout* const layout, const std::size_t field_index)
{
    if (layout == nullptr)
    {
        return ElementLayout{};
    }
    return ((layout->per_field != nullptr) && (field_index < layout->per_field_count)) ? layout->per_field[field_index]
                                                                                       : layout->default_;
}

inline LayoutCursor EnterField(const LayoutCursor& cursor, const std::size_t field_index)
{
    const ElementLayout leaf = ElementAt(cursor.struct_layout, field_index);
    return {leaf.nested, leaf};
}

inline LayoutCursor EnterArrayElement(const LayoutCursor& cursor)
{
    const SomeIpLayout* const nested = cursor.leaf.nested;
    return {nested, (nested != nullptr) ? nested->default_ : ElementLayout{}};
}

}  // namespace someip_detail

}  // namespace serialization
}  // namespace score

#endif  // SCORE_SERIALIZATION_STRATEGIES_SOMEIP_LAYOUT_H
