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
#include "score/mw/com/impl/e2e/e2e_profile_stub.h"

#include <cstddef>
#include <cstdint>

namespace score::mw::com::impl::e2e
{
namespace
{
// Offsets for the fields in the profile stub header.
constexpr std::size_t kDataIdOffset{0U};
constexpr std::size_t kCounterOffset{1U};
constexpr std::size_t kChecksumLowOffset{2U};
constexpr std::size_t kChecksumHighOffset{3U};

std::uint8_t ToUint8(const std::byte value) noexcept
{
    return std::to_integer<std::uint8_t>(value);
}

std::uint16_t CalculateChecksum(const std::uint8_t data_id,
                                const std::uint8_t counter,
                                const score::cpp::span<const std::byte> payload) noexcept
{
    std::uint32_t checksum{static_cast<std::uint32_t>(data_id) + static_cast<std::uint32_t>(counter)};
    for (const auto payload_byte : payload)
    {
        checksum += static_cast<std::uint32_t>(ToUint8(payload_byte));
    }
    return static_cast<std::uint16_t>(checksum & 0xFFFFU);
}

std::uint16_t ReadChecksum(const score::cpp::span<const std::byte> header) noexcept
{
    const auto low_byte = static_cast<std::uint16_t>(ToUint8(header[kChecksumLowOffset]));
    const auto high_byte = static_cast<std::uint16_t>(ToUint8(header[kChecksumHighOffset]));
    return static_cast<std::uint16_t>(low_byte | static_cast<std::uint16_t>(high_byte << 8U));
}

void WriteChecksum(score::cpp::span<std::byte> header, const std::uint16_t checksum) noexcept
{
    header[kChecksumLowOffset] = static_cast<std::byte>(checksum & 0x00FFU);
    header[kChecksumHighOffset] = static_cast<std::byte>((checksum >> 8U) & 0x00FFU);
}

}  // namespace

bool ProtectMessage(const score::cpp::span<std::byte> header,
                    const score::cpp::span<const std::byte> payload,
                    const ProfileConfiguration configuration,
                    ProtectContext& protect_context) noexcept
{
    if (header.size() != kProfileStubHeaderSize)
    {
        return false;
    }

    const auto counter = protect_context.next_counter;
    header[kDataIdOffset] = static_cast<std::byte>(configuration.data_id);
    header[kCounterOffset] = static_cast<std::byte>(counter);
    WriteChecksum(header, CalculateChecksum(configuration.data_id, counter, payload));
    protect_context.next_counter = static_cast<std::uint8_t>(counter + 1U);
    return true;
}

CheckOutcome CheckMessage(const score::cpp::span<const std::byte> header,
                          const score::cpp::span<const std::byte> payload,
                          const ProfileConfiguration configuration,
                          CheckContext& check_context) noexcept
{
    if (header.size() != kProfileStubHeaderSize)
    {
        return {DataIntegrityStatus::kError, SequenceStatus::kOk};
    }

    const auto counter = ToUint8(header[kCounterOffset]);
    const auto received_checksum = ReadChecksum(header);
    const auto expected_checksum = CalculateChecksum(configuration.data_id, counter, payload);
    if ((ToUint8(header[kDataIdOffset]) != configuration.data_id) || (received_checksum != expected_checksum))
    {
        return {DataIntegrityStatus::kError, SequenceStatus::kOk};
    }

    if (!check_context.has_last_counter)
    {
        check_context.last_counter = counter;
        check_context.has_last_counter = true;
        return {DataIntegrityStatus::kOk, SequenceStatus::kOk};
    }

    const auto delta = static_cast<std::uint8_t>(counter - check_context.last_counter);
    check_context.last_counter = counter;
    if (delta == 0U)
    {
        return {DataIntegrityStatus::kOk, SequenceStatus::kErrorRepeated};
    }
    if (delta == 1U)
    {
        return {DataIntegrityStatus::kOk, SequenceStatus::kOk};
    }
    if (delta <= configuration.max_delta_counter)
    {
        return {DataIntegrityStatus::kOk, SequenceStatus::kOkGapWithinThreshold};
    }
    return {DataIntegrityStatus::kOk, SequenceStatus::kErrorGapExceedsThreshold};
}

}  // namespace score::mw::com::impl::e2e