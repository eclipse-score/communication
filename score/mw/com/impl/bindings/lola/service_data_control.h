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
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_control.h"
#include "score/mw/com/impl/bindings/lola/uid_pid_mapping.h"

namespace score::mw::com::impl::lola
{
class ServiceDataControl
{
  public:
    /// \todo Instead of using a fixed value (50) for the number of uid-pid mappings, we should come up with some sane
    ///       calculation based on config settings and then hand over this calculated number ...
    static constexpr std::uint16_t kMaxUidPidMappings = 50U;

    /// \brief Ctor for the ServiceDataControl to place it in given shared memory resource identified via given
    ///        memory resource proxy.
    /// \details ServiceDataControl is designed to be located in shared memory, therefore the explicit
    ///          MemoryResourceProxy argument! (Yes one could come up with a MemoryResourceProxy pointing to a local
    ///          memory resource, but this would be "uncommon")
    /// \param proxy MemoryResourceProxy pointing to the memory-resource to be used

    explicit ServiceDataControl(const score::memory::shared::MemoryResourceProxy* const proxy)
        : event_controls_(proxy), uid_pid_mapping_(kMaxUidPidMappings, proxy)
    {
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    score::memory::shared::Map<ElementFqId, EventControl> event_controls_;

    /// \brief mapping of current proxy-application uid to their pid.
    /// \details Every proxy instance for this service shall register itself in this mapping.
    ///          It is also used by proxy instances to detect, whether they have crashed before. They would find their
    ///          uid already registered with a different (old) PID. Note: There can be special cases, where an
    ///          consumer/proxy application has several proxy instances for the very same service! In this case, they
    ///          would overwrite their registration for their uid with the same pid, which is ok.
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    UidPidMapping<score::memory::shared::PolymorphicOffsetPtrAllocator<UidPidMappingEntry>> uid_pid_mapping_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_CONTROL_H
