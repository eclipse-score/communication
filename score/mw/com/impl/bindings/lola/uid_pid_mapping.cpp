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
#include "score/mw/com/impl/bindings/lola/uid_pid_mapping.h"

#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"
#include "score/mw/log/logging.h"

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

namespace
{

/// \brief Iterates through the given entries and updates the pid for the given uid, if an entry with the given uid
///        exists and is in the right state.
/// \param entries_begin start iterator/pointer for the entries
/// \param entries_end end iterator/pointer for the entries
/// \param uid uid for which the pid shall be registered/updated
/// \param pid new pid
/// \return if the given uid has been found, either the old/previous pid is returned (in case status was in kUsed) or
///         the new pid is returned, if status was in kUpdating. If uid wasn't found empty optional gets returned.
std::optional<pid_t> TryUpdatePidForExistingUid(
    // Suppress "AUTOSAR C++14 A8-4-10" rule. The rule declares "A parameter shall be passed by reference
    // if it can’t be NULL".
    // We need this parameter as an iterator over a container, semantically it should never be a nullptr as
    // the DynamicArray always provides valid iterators from begin() / end().
    // coverity[autosar_cpp14_a8_4_10_violation]
    score::containers::DynamicArray<UidPidMappingEntry>::iterator entries_begin,
    score::containers::DynamicArray<UidPidMappingEntry>::iterator entries_end,
    const uid_t uid,
    const pid_t pid)
{
    // Suppress "AUTOSAR C++14 M5-0-15". This rule states:"indexing shall be the only form of pointer arithmetic.".
    // Rationale: Tolerated due to containers providing pointer-like iterators.
    // The Coverity tool considers these iterators as raw pointers.
    // coverity[autosar_cpp14_m5_0_15_violation]
    for (auto it = entries_begin; it != entries_end; it++)
    {
        // extract out into a separate function
        const auto status_uid = it->GetStatusAndUidAtomic();
        const auto entry_status = status_uid.first;
        const auto entry_uid = status_uid.second;
        if ((entry_status == UidPidMappingEntry::MappingEntryStatus::kUsed) && (entry_uid == uid))
        {
            // uid already exists. It is "owned" by us, so we can directly update pid, without atomic state changes ...
            pid_t old_pid = it->pid_;
            it->pid_ = pid;
            return old_pid;
        }
        else if ((entry_status == UidPidMappingEntry::MappingEntryStatus::kUpdating) && (entry_uid == uid))
        {
            // This is a very odd situation! I.e. someone is currently updating the pid for OUR uid!
            // this could only be possible, when our uid/client app has crashed before, while updating the pid
            // for our uid.
            score::mw::log::LogWarn("lola")
                << "UidPidMapping: Found mapping entry for own uid in state kUpdating. Maybe we "
                   "crashed before!? Now taking over entry and updating with current PID.";
            it->pid_ = pid;
            it->SetStatusAndUidAtomic(UidPidMappingEntry::MappingEntryStatus::kUsed, uid);
            return pid;
        }
    }
    return {};
}

}  // namespace
namespace detail
{

// Suppress "AUTOSAR C++14 M3-2-3" rule finding. The rule states: "The One Definition Rule shall not be violated."
// This is a template function which is declared in the header and defined here in the cpp file only once.
// We are aware that implementing templates in header files in C++ is a requirement due to how templates are compiled
// and instantiated. However, we do NOT want to expose this code to the user for encapsulation reasons.
// Suppress "AUTOSAR C++14 A8-4-10" rule. The rule declares "A parameter shall be passed by reference if it can’t be
// NULL". We need this parameter as an iterator over a container, semantically it should never be a nullptr as
// the DynamicArray always provides valid iterators from begin() / end().
template <template <class> class AtomicIndirectorType>
// coverity[autosar_cpp14_m3_2_3_violation]
// coverity[autosar_cpp14_a8_4_10_violation]
std::optional<pid_t> RegisterPid(score::containers::DynamicArray<UidPidMappingEntry>::iterator entries_begin,
                                 score::containers::DynamicArray<UidPidMappingEntry>::iterator entries_end,
                                 const uid_t uid,
                                 const pid_t pid)
{
    const auto result_pid = TryUpdatePidForExistingUid(entries_begin, entries_end, uid, pid);
    if (result_pid.has_value())
    {
        return result_pid;
    }

    // \todo find a "sane" value for max-retries!
    constexpr std::size_t kMaxRetries{50U};
    std::size_t retry_count{0U};
    while (retry_count < kMaxRetries)
    {
        retry_count++;
        // Suppress "AUTOSAR C++14 M5-0-15". This rule states:"indexing shall be the only form of pointer arithmetic.".
        // Rationale: Tolerated due to containers providing pointer-like iterators.
        // The Coverity tool considers these iterators as raw pointers.
        // coverity[autosar_cpp14_m5_0_15_violation]
        for (auto it = entries_begin; it != entries_end; it++)
        {
            const auto status_uid = it->GetStatusAndUidAtomic();
            const auto entry_status = status_uid.first;
            const auto entry_uid = status_uid.second;

            if (entry_status == UidPidMappingEntry::MappingEntryStatus::kUnused)
            {
                auto current_entry_key = UidPidMappingEntry::CreateKey(entry_status, entry_uid);
                auto new_entry_key =
                    UidPidMappingEntry::CreateKey(UidPidMappingEntry::MappingEntryStatus::kUpdating, uid);

                if (AtomicIndirectorType<UidPidMappingEntry::key_type>::compare_exchange_weak(
                        it->key_uid_status_, current_entry_key, new_entry_key, std::memory_order_acq_rel))
                {
                    it->pid_ = pid;
                    it->SetStatusAndUidAtomic(UidPidMappingEntry::MappingEntryStatus::kUsed, uid);
                    return pid;
                }
            }
        }
    }
    return {};
}

/// Instantiate the method templates once for AtomicIndirectorReal (production use) and AtomicIndirectorMock (testing)
/// Those are all instantiations we need, so we are able to put the template definitions into the .cpp file.
template std::optional<pid_t> RegisterPid<memory::shared::AtomicIndirectorMock>(
    score::containers::DynamicArray<UidPidMappingEntry>::iterator entries_begin,
    score::containers::DynamicArray<UidPidMappingEntry>::iterator entries_end,
    const uid_t uid,
    const pid_t pid);
template std::optional<pid_t> RegisterPid<memory::shared::AtomicIndirectorReal>(
    score::containers::DynamicArray<UidPidMappingEntry>::iterator entries_begin,
    score::containers::DynamicArray<UidPidMappingEntry>::iterator entries_end,
    const uid_t uid,
    const pid_t pid);

}  // namespace detail
}  // namespace score::mw::com::impl::lola
