/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#ifndef BASELIBS_SCORE_MEMORY_ENDIANNESS_H
#define BASELIBS_SCORE_MEMORY_ENDIANNESS_H

#include <cstdint>

namespace score::memory
{

/// @brief Represents the possible memory layouts a computer system can have
enum class Endianness : std::uint8_t
{
    kLittleEndian,
    kBigEndian
};

/// @brief By interpreting the memory of an integer, determine which endianness the current system uses (on runtime)
Endianness DetermineSystemEndianness() noexcept;

// @brief Determine on runtime if the current system is running as little endian
bool IsSystemLittleEndian() noexcept;

// @brief Determine on runtime if the current system is running as big endian
bool IsSystemBigEndian() noexcept;

/// @brief Helper function to check if bytes need to be switched in order
/// @param required_endianness The endianness required (e.g. by requirements on the network)
/// @return bool if switching bytes is necessary in order to achieve required endianness, false otherwise
bool SwitchingBytesNecessary(const Endianness required_endianness) noexcept;

/// @brief Reverses the bytes in the given integer value
/// @note Within C++23 this can be replaced with https://en.cppreference.com/w/cpp/numeric/byteswap
/// @param value        The value to convert
/// @return The converted value
constexpr std::uint32_t ByteSwap(const std::uint32_t value) noexcept
{
    return ((value >> 24U) & 0x000000FFU) | /*!< byte 3 to byte 0 */
           ((value >> 8U) & 0x0000FF00U) |  /*!< byte 2 to byte 1 */
           ((value << 8U) & 0x00FF0000U) |  /*!< byte 1 to byte 2 */
           ((value << 24U) & 0xFF000000U);  /*!< byte 0 to byte 3 */
}

/// @brief Reverses the bytes in the given integer value
/// @note Within C++23 this can be replaced with https://en.cppreference.com/w/cpp/numeric/byteswap
/// @param value        The value to convert
/// @return The converted value
constexpr std::uint64_t ByteSwap(const std::uint64_t value) noexcept
{
    return static_cast<std::uint64_t>((value >> 56U) & 0x00000000000000FFULL) | /*!< byte 7 to byte 0 */
           static_cast<std::uint64_t>((value >> 40U) & 0x000000000000FF00ULL) | /*!< byte 6 to byte 1 */
           static_cast<std::uint64_t>((value >> 24U) & 0x0000000000FF0000ULL) | /*!< byte 5 to byte 2 */
           static_cast<std::uint64_t>((value >> 8U) & 0x00000000FF000000ULL) |  /*!< byte 4 to byte 3 */
           static_cast<std::uint64_t>((value << 8U) & 0x000000FF00000000ULL) |  /*!< byte 3 to byte 4 */
           static_cast<std::uint64_t>((value << 24U) & 0x0000FF0000000000ULL) | /*!< byte 2 to byte 5 */
           static_cast<std::uint64_t>((value << 40U) & 0x00FF000000000000ULL) | /*!< byte 1 to byte 6 */
           static_cast<std::uint64_t>((value << 56U) & 0xFF00000000000000ULL);  /*!< byte 0 to byte 7 */
}

/// @brief Converts the byte encoding of integer values from the big-endian byte order to the current CPU (the "host")
/// byte order uses
/// @param value    The value to convert
/// @return The converted value
template <typename ValueType>
auto BigEndianToHostEndianness(const ValueType value) noexcept -> ValueType
{
    /* Negative test: Depends on the host system, cannot be covered in current setups. Which is no problem,
       since this is only the other half of a condition. */
    if (IsSystemLittleEndian())  // LCOV_EXCL_BR_LINE
    {
        return ByteSwap(value);
    }
    return value;  // LCOV_EXCL_LINE
}

}  // namespace score::memory

#endif  // BASELIBS_SCORE_MEMORY_ENDIANNESS_H
