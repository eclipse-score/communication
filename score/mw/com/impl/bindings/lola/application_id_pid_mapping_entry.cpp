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
#include "score/mw/com/impl/bindings/lola/application_id_pid_mapping_entry.h"

#include <cstdint>

namespace score::mw::com::impl::lola
{

std::pair<ApplicationIdPidMappingEntry::MappingEntryStatus, std::uint32_t>
ApplicationIdPidMappingEntry::GetStatusAndApplicationIdAtomic() noexcept
{
    constexpr std::uint64_t kMaskApplicationId = 0x00000000FFFFFFFFU;
    // MappingEntryStatus has an underlying type of std::uint16_t (see CreateKey() below, which only ever stores a
    // std::uint16_t-sized status in bits 32..47 of the key). Masking to 16 bits here (in addition to the shift)
    // makes the value range of status_part provably fit the enum's underlying type, avoiding an out-of-range enum
    // cast (CodeQL cpp/misra/out-of-range-enum-cast-critical-unspecified-behavior).
    constexpr std::uint64_t kMaskStatus = 0x000000000000FFFFU;
    auto status_applictionid = key_application_id_status_.load();
    static_assert(std::is_same<std::uint16_t,
                               std::underlying_type<ApplicationIdPidMappingEntry::MappingEntryStatus>::type>::value,
                  "MappingEntryStatus is not of expected underlying type");
    const auto status_part = static_cast<std::uint16_t>((status_applictionid >> 32U) & kMaskStatus);
    const auto application_id_part = static_cast<std::uint32_t>(status_applictionid & kMaskApplicationId);
    // Suppress "AUTOSAR C++14 A7-2-1" rule: "An expression with enum underlying type shall only have values
    // corresponding to the enumerators of the enumeration.".
    // Callers of this function process only the MappingEntryStatus enumeration values ​​of interest, the rest are
    // discarded.
    // coverity[autosar_cpp14_a7_2_1_violation]
    return {static_cast<ApplicationIdPidMappingEntry::MappingEntryStatus>(status_part), application_id_part};
}

void ApplicationIdPidMappingEntry::SetStatusAndApplicationIdAtomic(MappingEntryStatus status,
                                                                   std::uint32_t application_id) noexcept
{
    key_application_id_status_.store(CreateKey(status, application_id));
}

ApplicationIdPidMappingEntry::key_type ApplicationIdPidMappingEntry::CreateKey(MappingEntryStatus status,
                                                                               std::uint32_t application_id) noexcept
{
    ApplicationIdPidMappingEntry::key_type result = static_cast<std::uint16_t>(status);
    result = result << 32U;
    result |= static_cast<std::uint64_t>(application_id);
    return result;
}

}  // namespace score::mw::com::impl::lola
