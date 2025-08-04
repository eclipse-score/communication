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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_APPLICATION_ID_PID_MAPPING_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_APPLICATION_ID_PID_MAPPING_H

#include "score/mw/com/impl/bindings/lola/register_pid_fake.h"
#include "score/mw/com/impl/bindings/lola/application_id_pid_mapping_entry.h"

#include "score/containers/dynamic_array.h"
#include "score/memory/shared/atomic_indirector.h"

#include <sys/types.h>
#include <cstdint>
#include <memory>
#include <optional>

namespace score::mw::com::impl::lola
{

namespace detail
{

/// \brief implementation for ApplicationIdPidMapping::RegisterPid, which allows selecting the AtomicIndirectorType for
///        testing purpose.
/// \tparam AtomicIndirectorType allows mocking of atomic operations done by this method.
/// \see ApplicationIdPidMapping::RegisterPid

// Suppress "AUTOSAR C++14 M3-2-3" rule finding. This rule states: "A type, object or function that is used in multiple
// translation units shall be declared in one and only one file.".
// coverity[autosar_cpp14_m3_2_3_violation] This is false positive. Function is declared only once.
template <template <class> class AtomicIndirectorType = memory::shared::AtomicIndirectorReal>
// coverity[autosar_cpp14_m3_2_3_violation] This is false positive. Function is declared only once.
std::optional<pid_t> RegisterPid(score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_begin,
                                 score::containers::DynamicArray<ApplicationIdPidMappingEntry>::iterator entries_end,
                                 const uid_t application_id,
                                 const pid_t pid);

}  // namespace detail

/// \brief class holding application identifier to pid mappings for a concrete service instance.
/// \details an instance of this class is stored in shared-memory within a given ServiceDataControl, which represents a
///          concrete service instance. The ServiceDataControl and its ApplicationIdPidMapping member are created by the provider/
///          skeleton instance. The ApplicationIdPidMapping is then populated (registrations done) by the proxy instances, which
///          use this service instance. So each proxy instance (contained within a proxy-process) registers its
///          application identifier (which is either an explicitly configured 'applicationID' or the process's uid as a
///          fallback) together with
///          its current pid in this map.
///          In the rare case, that there are multiple proxy instances within the same process, which use the same
///          service instance, it is ensured that only the 1st/one of the proxies does this registration.
///          These registrations are then later used by a proxy application in a restart after crash. A proxy instance
///          at its creation will get back its previous pid, when it registers itself and has been previously registered. If the proxy instance does get back such a previous pid, it notifies the provider/skeleton
///          side, that this is an old/outdated pid, where the provider side shall then clean-up/remove any (message
///          passing) artefacts related to the old pid.
/// \tparam Allocator allocator to be used! In production code we store instances of this class in shared-memory, so in
///         this case our PolymorphicOffsetPtrAllocator gets used.
template <typename Allocator>
class ApplicationIdPidMapping
{
  public:
    /// \brief Create a ApplicationIdPidMapping instance with a capacity of up to max_mappings mappings for application IDs.
    /// \param alloc allocator to be used to allocate the mapping data structure
    ApplicationIdPidMapping(std::uint16_t max_mappings, const Allocator& alloc) : mapping_entries_(max_mappings, alloc){};

    /// \brief Registers the given pid for the given application ID. Eventually overwriting an existing mapping for this ID.
    /// \attention We intentionally do NOT provide an unregister functionality. Semantically an unregister is not
    ///            needed. If we would correctly implement an unregister, we would need to care for correctly tracking
    ///            all the proxy instances in the local process and do the removal of a application ID-pid mapping, when the last
    ///            proxy instance related to this service-instance/ApplicationIdPidMapping has been destructed.
    ///            This is complex because the ApplicationIdPidMapping data-structure is placed in shared-memory and access to it
    ///            from various different (proxy)processes is synchronized via atomic-lock-free algo. The additional
    ///            synchronization for the seldom use-case of multiple proxy-instances within one process accessing the
    ///            same service-instance would need a much more complex sync, which we skipped for now.
    ///            The main downside is: In case a proxy process restarts normally (no crash) and then connects to the
    ///            same service instance again, which stayed active, it will during ApplicationIdPidMapping::RegisterPid() get
    ///            back its old pid again (since it was not unregistered) and will inform the skeleton side about this
    ///            old/outdated pid. This notification isn't really needed in case of a previous clean shutdown of the
    ///            proxy process, since in case of a clean shutdown things like event-receive-handlers have been
    ///            correctly deregistered.
    ///
    /// \param application_id ID identifying application for which its current pid is registered
    /// \param pid current pid to register
    /// \return if the application ID had a previous mapping to a pid, the old pid will be returned. If there wasn't yet a mapping
    ///         for the pid, the new pid is returned. If the registration/mapping couldn't be done (no space left) an
    ///         empty optional will be returned.
    std::optional<pid_t> RegisterPid(const uid_t application_id, const pid_t pid)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(nullptr != mapping_entries_.begin());
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(nullptr != mapping_entries_.end());

        if (register_pid_fake_ != nullptr)
        {
            return register_pid_fake_->RegisterPid(mapping_entries_.begin(), mapping_entries_.end(), application_id, pid);
        }

        // Suppress the rule AUTOSAR C++14 A5-3-2: "Null pointers shall not be dereferenced.".
        // Whereas, both begin() and end() iterators of "mapping_entries_" (DynamicArray type) return pointer to
        // element. Therefore, the provided assertions check this rule requirement, whether the iterators return a
        // result other than nullptr.
        // coverity[autosar_cpp14_a5_3_2_violation]
        return detail::RegisterPid<memory::shared::AtomicIndirectorReal>(
            mapping_entries_.begin(), mapping_entries_.end(), application_id, pid);
    };

    static void InjectRegisterPidFake(RegisterPidFake& register_pid_fake)
    {
        register_pid_fake_ = &register_pid_fake;
    }

    static void ClearRegisterPidFake()
    {
        register_pid_fake_ = nullptr;
    }

  private:
    using mapping_entry_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<ApplicationIdPidMappingEntry>;

    score::containers::DynamicArray<ApplicationIdPidMappingEntry, mapping_entry_alloc> mapping_entries_;
    static RegisterPidFake* register_pid_fake_;
};

template <typename Allocator>
// A3-1-1: "It shall be possible to include any header file in multiple
// translation units without violating the One Definition Rule."
// false-positive: it's the correct syntax for defining a static member variable of a template class
// see https://en.cppreference.com/w/cpp/language/variable_template
// coverity[autosar_cpp14_a3_1_1_violation]
RegisterPidFake* ApplicationIdPidMapping<Allocator>::register_pid_fake_{nullptr};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_APPLICATION_ID_PID_MAPPING_H