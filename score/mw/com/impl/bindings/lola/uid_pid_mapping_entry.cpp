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
#include "score/mw/com/impl/bindings/lola/uid_pid_mapping_entry.h"

#include <cstdint>

namespace score::mw::com::impl::lola
{

std::pair<UidPidMappingEntry::MappingEntryStatus, uid_t> UidPidMappingEntry::GetStatusAndUidAtomic() noexcept
{
    constexpr std::uint64_t kMaskUid = 0x00000000FFFFFFFFU;
    auto status_uid = key_uid_status_.load();
    const auto status_part = static_cast<std::uint32_t>(status_uid >> 32U);
    const auto uid_part = static_cast<std::uint32_t>(status_uid & kMaskUid);
    // Suppress "AUTOSAR C++14 A7-2-1" rule: "An expression with enum underlying type shall only have values
    // corresponding to the enumerators of the enumeration.".
    // Callers of this function process only the MappingEntryStatus enumeration values ​​of interest, the rest are
    // discarded.
    // coverity[autosar_cpp14_a7_2_1_violation]
    return {static_cast<UidPidMappingEntry::MappingEntryStatus>(status_part), static_cast<uid_t>(uid_part)};
}

void UidPidMappingEntry::SetStatusAndUidAtomic(MappingEntryStatus status, uid_t uid) noexcept
{
    key_uid_status_.store(CreateKey(status, uid));
}

UidPidMappingEntry::key_type UidPidMappingEntry::CreateKey(MappingEntryStatus status, uid_t uid) noexcept
{
    UidPidMappingEntry::key_type result = static_cast<std::uint16_t>(status);
    result = result << 32U;
    static_assert(sizeof(uid) <= 4, "For more than 32 bits we cannot guarantee the key to be unique");
// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Need to limit this functionality to QNX only.
// coverity[autosar_cpp14_a16_0_1_violation]
#ifdef __QNX__
    // In QNX uid is signed. Technically speaking, given the type, the uid could be negative. In any case it does not
    // matter, we just need a value to be used as a key and needs to be converted always in the same way.
    const auto fixed_size_uid = static_cast<std::uint32_t>(uid);
// coverity[autosar_cpp14_a16_0_1_violation]  Different implementation required for linux.
#else
    const std::uint32_t fixed_size_uid{uid};
// coverity[autosar_cpp14_a16_0_1_violation]
#endif
    result |= fixed_size_uid;
    return result;
}

}  // namespace score::mw::com::impl::lola
