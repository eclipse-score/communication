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
#include "score/memory/endianness.h"

#include <score/utility.hpp>

#include <cstring>

namespace score::memory
{

Endianness DetermineSystemEndianness() noexcept
{
    constexpr std::uint16_t value{0x0A0BU};
    std::uint8_t part_of_value{0xFFU};

    // We do not do any reinterpret cast or anything, since this would be undefined behavior
    // No risk of using banned memcpy function here that would cause UB, because passed objects are trivial types and
    // minimal bytes specified to copy.
    // NOLINTNEXTLINE(score-banned-function): Tolerated unless no alternative
    score::cpp::ignore = std::memcpy(&part_of_value, &value, 1U);

    /* Negative test: Depends on the host system, cannot be covered in current setups. Which is no problem,
       since this is only the other half of a condition. */
    if (part_of_value == 0x0BU)  // LCOV_EXCL_BR_LINE
    {
        return Endianness::kLittleEndian;
    }
    else
    {
        return Endianness::kBigEndian;  // LCOV_EXCL_LINE
    }
}

// @brief Determine on runtime if the current system is running as little endian
bool IsSystemLittleEndian() noexcept
{
    return DetermineSystemEndianness() == Endianness::kLittleEndian;
}

// @brief Determine on runtime if the current system is running as big endian
bool IsSystemBigEndian() noexcept
{
    return DetermineSystemEndianness() == Endianness::kBigEndian;
}

/// @brief Helper function to check if bytes need to be switched in order
/// @param required_endianness The endianness required (e.g. by requirements on the network)
/// @return bool if switching bytes is necessary in order to achieve required endianness, false otherwise
bool SwitchingBytesNecessary(const Endianness required_endianness) noexcept
{
    return required_endianness != DetermineSystemEndianness();
}

}  // namespace score::memory
