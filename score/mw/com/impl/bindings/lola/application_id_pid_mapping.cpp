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
#include "score/mw/com/impl/bindings/lola/application_id_pid_mapping.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl::lola
{

namespace
{

/// \brief Iterates through the given entries and updates the pid for the given application ID, if an entry with the
/// given ID
///        exists and is in the right state.
/// \param entries_begin start iterator/pointer for the entries
/// \param entries_end end iterator/pointer for the entries
/// \param application_id application ID for which the pid shall be registered/updated
/// \param pid new pid
/// \return if the given application ID has been found, either the old/previous pid is returned (in case status was in
/// kUsed) or
///         the new pid is returned, if status was in kUpdating. If application ID wasn't found empty optional gets
///         returned.
std::optional<pid_t> TryUpdatePidForExistingId(
    // Suppress "AUTOSAR C++14 A8-4-10" rule. The rule declares "A parameter shall be passed by reference
    // if it can’t be NULL".
    // We need this parameter as an iterator over a container, semantically it should never be a nullptr as
    // the DynamicArray always provides valid iterators from begin() / end().
    // coverity[autosar_cpp14_a8_4_10_violation]
    score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_begin,
    score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_end,
    const std::uint32_t application_id,
    const pid_t pid)
{
    // Suppress "AUTOSAR C++14 M5-0-15". This rule states:"indexing shall be the only form of pointer arithmetic.".
    // Rationale: Tolerated due to containers providing pointer-like iterators.
    // The Coverity tool considers these iterators as raw pointers.
    // coverity[autosar_cpp14_m5_0_15_violation]
    for (auto it = entries_begin; it != entries_end; it++)
    {
        // extract out into a separate function
        const auto status_applicationid = it->GetStatusAndApplicationIdAtomic();
        const auto entry_status = status_applicationid.first;
        const auto entry_applicationid = status_applicationid.second;
        if ((entry_status == ApplicationIdPidMappingEntry::MappingEntryStatus::kUsed) &&
            (entry_applicationid == application_id))
        {
            // application ID already exists. It is "owned" by us, so we can directly update pid, without atomic state
            // changes ...
            pid_t old_pid = it->pid_;
            it->pid_ = pid;
            return old_pid;
        }
        else if ((entry_status == ApplicationIdPidMappingEntry::MappingEntryStatus::kUpdating) &&
                 (entry_applicationid == application_id))
        {
            // This is a very odd situation! I.e. someone is currently updating the pid for OUR application ID!
            // this could only be possible, when our application has crashed before, while updating the pid
            // for our application ID.
            score::mw::log::LogWarn("lola")
                << "ApplicationIdPidMapping: Found mapping entry for own application ID in state kUpdating. Maybe we "
                   "crashed before!? Now taking over entry and updating with current PID.";
            it->pid_ = pid;
            it->SetStatusAndApplicationIdAtomic(ApplicationIdPidMappingEntry::MappingEntryStatus::kUsed,
                                                application_id);
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
std::optional<pid_t> RegisterPid(score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_begin,
                                 score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_end,
                                 const std::uint32_t application_id,
                                 const pid_t pid)
{
    const auto result_pid = TryUpdatePidForExistingId(entries_begin, entries_end, application_id, pid);
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
            const auto status_applicationid = it->GetStatusAndApplicationIdAtomic();
            const auto entry_status = status_applicationid.first;
            const auto entry_applicationid = status_applicationid.second;

            if (entry_status == ApplicationIdPidMappingEntry::MappingEntryStatus::kUnused)
            {
                auto current_entry_key = ApplicationIdPidMappingEntry::CreateKey(entry_status, entry_applicationid);
                auto new_entry_key = ApplicationIdPidMappingEntry::CreateKey(
                    ApplicationIdPidMappingEntry::MappingEntryStatus::kUpdating, application_id);

                if (AtomicIndirectorType<ApplicationIdPidMappingEntry::key_type>::compare_exchange_weak(
                        it->key_application_id_status_, current_entry_key, new_entry_key, std::memory_order_acq_rel))
                {
                    it->pid_ = pid;
                    it->SetStatusAndApplicationIdAtomic(ApplicationIdPidMappingEntry::MappingEntryStatus::kUsed,
                                                        application_id);
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
    score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_begin,
    score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_end,
    const std::uint32_t application_id,
    const pid_t pid);
template std::optional<pid_t> RegisterPid<memory::shared::AtomicIndirectorReal>(
    score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_begin,
    score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_end,
    const std::uint32_t application_id,
    const pid_t pid);

}  // namespace detail
}  // namespace score::mw::com::impl::lola
