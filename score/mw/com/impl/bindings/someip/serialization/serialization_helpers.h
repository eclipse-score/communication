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
 *******************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZATION_HELPERS_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZATION_HELPERS_H

#include <score/span.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace score::mw::com::impl::someip::serialization
{

/// \brief Wire byte order for multi-byte scalars.
///
/// Each multi-byte base type may be encoded independently, so the byte order is
/// selected per value rather than applied globally. Every multi-byte helper
/// therefore takes the order explicitly.
enum class ByteOrder : std::uint8_t
{
    kBigEndian,     ///< Network byte order / most significant byte first.
    kLittleEndian,  ///< Least significant byte first.
};

// -----------------------------------------------------------------------------
// Boolean
//
// Occupies a single byte with FALSE encoded as 0x00 and TRUE as 0x01. When
// decoding, only the least significant bit is evaluated and the remaining bits
// are disregarded.
// -----------------------------------------------------------------------------

/// \brief Writes value as a single boolean byte (out.size() == 1).
void WriteBool(bool value, score::cpp::span<std::uint8_t> out) noexcept;

/// \brief Reads a boolean byte, evaluating only its least significant bit
///        (in.size() == 1).
bool ReadBool(score::cpp::span<const std::uint8_t> in) noexcept;

// -----------------------------------------------------------------------------
// Fixed-width scalars (8/16/32/64-bit integers, signed or unsigned, and
// IEEE-754 binary32/binary64 floating point)
//
// A single generic template pair handles every fixed-width base type. The value
// is processed byte by byte, so the result is independent of the host
// endianness and of the destination's memory alignment (bytes are addressed by
// offset). For single-byte types the byte order has no effect.
// -----------------------------------------------------------------------------

namespace detail
{

/// \brief Unsigned integer type used to hold a scalar's object representation
///        while its bytes are re-ordered. For integers this is simply the
///        same-width unsigned type; for floating point it is a same-width
///        unsigned integer holding the IEEE-754 bit pattern.
template <typename T, typename = void>
struct ScalarBits;

template <typename T>
struct ScalarBits<T, std::enable_if_t<std::is_integral<T>::value>>
{
    using type = std::make_unsigned_t<T>;
};

template <>
struct ScalarBits<float>
{
    static_assert(sizeof(float) == 4U, "float must be IEEE-754 binary32");
    using type = std::uint32_t;
};

template <>
struct ScalarBits<double>
{
    static_assert(sizeof(double) == 8U, "double must be IEEE-754 binary64");
    using type = std::uint64_t;
};

template <typename T>
using ScalarBitsT = typename ScalarBits<T>::type;

/// \brief Reinterprets a value's object representation as a same-width unsigned
///        integer, preserving the exact bit pattern (two's complement for
///        integers, IEEE-754 for floating point).
template <typename T>
ScalarBitsT<T> ToBits(const T value) noexcept
{
    ScalarBitsT<T> bits{0};
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/// \brief Inverse of ToBits: rebuilds T from its unsigned bit pattern.
template <typename T>
T FromBits(const ScalarBitsT<T> bits) noexcept
{
    T value{0};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace detail

/// \brief Encodes a scalar of type T into out using the given byte order
///        (out.size() == sizeof(T)). T is an integer or floating-point type.
template <typename T>
void WriteScalar(const T value, const ByteOrder order, const score::cpp::span<std::uint8_t> out) noexcept
{
    static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
                  "WriteScalar requires an integer or floating-point type");
    using RawBits = detail::ScalarBitsT<T>;
    const RawBits bits = detail::ToBits<T>(value);
    constexpr std::size_t width = sizeof(T);
    for (std::size_t i = 0U; i < width; ++i)
    {
        // Byte index counting from the most significant byte.
        const std::size_t shift = (width - 1U - i) * 8U;
        const auto byte = static_cast<std::uint8_t>((bits >> shift) & static_cast<RawBits>(0xFFU));
        const std::size_t pos = (order == ByteOrder::kBigEndian) ? i : (width - 1U - i);
        out[pos] = byte;
    }
}

/// \brief Decodes a scalar of type T from in using the given byte order
///        (in.size() == sizeof(T)). T is an integer or floating-point type.
template <typename T>
T ReadScalar(const ByteOrder order, const score::cpp::span<const std::uint8_t> in) noexcept
{
    static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
                  "ReadScalar requires an integer or floating-point type");
    using RawBits = detail::ScalarBitsT<T>;
    RawBits bits{0};
    constexpr std::size_t width = sizeof(T);
    for (std::size_t i = 0U; i < width; ++i)
    {
        const std::size_t shift = (width - 1U - i) * 8U;
        const std::size_t pos = (order == ByteOrder::kBigEndian) ? i : (width - 1U - i);
        bits = static_cast<RawBits>(bits | (static_cast<RawBits>(in[pos]) << shift));
    }
    return detail::FromBits<T>(bits);
}

}  // namespace score::mw::com::impl::someip::serialization

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZATION_HELPERS_H
