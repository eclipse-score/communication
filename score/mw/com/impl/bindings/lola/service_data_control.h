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

#include "score/mw/com/impl/bindings/lola/application_id_pid_mapping.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_control.h"
#include "score/mw/com/impl/bindings/lola/linear_search_map.h"

#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <score/span.hpp>

#include <cstddef>

namespace score::mw::com::impl::lola
{
class ServiceDataControl
{
  public:
    /// \todo Instead of using a fixed value (50) for the number of application-id to pid mappings, we should come up
    /// with some sane
    ///       calculation based on config settings and then hand over this calculated number ...
    static constexpr std::uint16_t kMaxApplicationIdPidMappings = 50U;

    /// \brief Ctor for the ServiceDataControl with a given memory resource to be used.
    /// \details ServiceDataControl is designed to be located in shared memory, therefore the explicit
    ///          ManagedMemoryResource argument!
    /// \param number_of_events_and_fields the (fixed) number of events + fields this service-instance provides. It is
    ///        used as the fixed capacity of the event_controls_ container. Since event_controls_ uses a
    ///        fixed-capacity container (LinearSearchMap), its capacity must be known at construction time.
    /// \param resource ManagedMemoryResource used for allocating underlying storage

    explicit ServiceDataControl(const std::size_t number_of_events_and_fields,
                                score::memory::shared::ManagedMemoryResource& resource)
        : event_controls_(number_of_events_and_fields, resource),
          application_id_pid_mapping_(kMaxApplicationIdPidMappings, resource)
    {
    }

    ~ServiceDataControl() noexcept = default;

    ServiceDataControl(const ServiceDataControl&) = delete;
    ServiceDataControl& operator=(const ServiceDataControl&) = delete;
    ServiceDataControl(ServiceDataControl&&) noexcept = delete;
    ServiceDataControl& operator=(ServiceDataControl&& other) noexcept = delete;

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    LinearSearchMap<ElementFqId, EventControl> event_controls_;

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

/// \brief Per service-element (event/field) information required to analytically size a ServiceDataControl.
struct ServiceElementControlSizeInfo
{
    /// \brief Number of event-data slots the service-element provides.
    std::size_t number_of_slots;
    /// \brief Maximum number of subscribers configured for the service-element.
    std::size_t max_subscribers;
};

/// \brief Analytically calculates the exact number of bytes a (single) ServiceDataControl (a control shm-object)
///        occupies.
/// \details This is used by SkeletonMemoryManager, but located next to ServiceDataControl so that the layout-dependent
///          size algorithm stays coupled to the data structure it reasons about.
///          It does NOT allocate any memory nor construct a ServiceDataControl; the size is
///          derived purely from the (fixed) container capacities.
///          The same size applies to the QM and (if present) the ASIL-B control shm-object, as both hold a
///          ServiceDataControl created with the very same configuration.
///          The result is exact (not just a bound): since all our allocations happen strictly sequentially (single
///          threaded, on a (monotonic) shared-memory resource which always starts allocating at a std::max_align_t
///          aligned location) and we know the size/alignment/order of every individual allocation performed by the
///          real construction, we can reconstruct the exact same sequence of allocations here and compute the exact
///          alignment-padding between them (see score::memory::shared::CalculateAlignedSizeOfSequence()).
/// \param events_and_fields_size_info per service-element sizing information. Its size equals the number of
///        service-elements (events + fields), which is the fixed capacity the event_controls_ container is constructed
///        with.
/// \return the exact number of bytes needed for a single control shm-object.
std::size_t CalculateServiceDataControlShmSize(
    score::cpp::span<const ServiceElementControlSizeInfo> events_and_fields_size_info);

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_CONTROL_H
