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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_CONTROL_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_CONTROL_H

#include "score/memory/shared/map.h"
#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"
#include "score/mw/com/impl/bindings/lola/application_id_pid_mapping.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_control.h"

namespace score::mw::com::impl::lola
{
class ServiceDataControl
{
  public:
    /// \todo Instead of using a fixed value (50) for the number of application-id to pid mappings, we should come up
    /// with some sane
    ///       calculation based on config settings and then hand over this calculated number ...
    static constexpr std::uint16_t kMaxApplicationIdPidMappings = 50U;

    /// \brief Ctor for the ServiceDataControl to place it in given shared memory resource identified via given
    ///        memory resource proxy.
    /// \details ServiceDataControl is designed to be located in shared memory, therefore the explicit
    ///          MemoryResourceProxy argument! (Yes one could come up with a MemoryResourceProxy pointing to a local
    ///          memory resource, but this would be "uncommon")
    /// \param proxy MemoryResourceProxy pointing to the memory-resource to be used

    explicit ServiceDataControl(const score::memory::shared::MemoryResourceProxy* const proxy)
        : event_controls_(proxy), application_id_pid_mapping_(kMaxApplicationIdPidMappings, proxy)
    {
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::memory::shared::Map<ElementFqId, EventControl> event_controls_;

    /// \brief mapping of a proxy's application identifier to its process ID (pid).
    /// \details Every proxy instance for this service shall register itself in this mapping. The identifier used is
    ///          either the 'applicationID' from the global configuration or, if not provided, the process's user ID
    ///          (uid) as a fallback. This mapping is used by proxy instances to detect if they have crashed. Upon
    ///          restart, they would find their application identifier already registered with a different (old) PID.
    ///          Note: In the special case where a consumer application has multiple proxy instances for the very same
    ///          service, they would all use the same application identifier and overwrite the registration with the
    ///          same pid, which is acceptable.
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    ApplicationIdPidMapping<score::memory::shared::PolymorphicOffsetPtrAllocator<ApplicationIdPidMappingEntry>>
        application_id_pid_mapping_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_CONTROL_H
