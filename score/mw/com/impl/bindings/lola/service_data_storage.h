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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_STORAGE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_STORAGE_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_meta_info.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/linear_search_map.h"
#include "score/mw/com/impl/runtime.h"

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/os/unistd.h"

#include <score/span.hpp>

#include <cstddef>

namespace score::mw::com::impl::lola
{

class ServiceDataStorage
{
  public:
    /// \brief associative container mapping a service-element (event/field) to the raw storage of its event-data slots.
    /// \details The value-type of the map is a type-erased pointer to the raw storage of the event-data slots.
    ///          The OffsetPtr points to a EventDataStorage<SampleType>, which gets created by events/fields, when
    ///          calling Skeleton::Register()!
    ///
    using EventDataStorageMap = LinearSearchMap<ElementFqId, score::memory::shared::OffsetPtr<void>>;
    /// \brief associative container mapping a service-element (event/field) to its (type-erased) meta-information.
    using EventMetaInfoMap = LinearSearchMap<ElementFqId, EventMetaInfo>;

    /// \brief Ctor for the ServiceDataStorage with a given memory resource to be used for internal storage allocation.
    /// \details ServiceDataStorage no longer uses dynamically allocating map-types. Instead it uses fixed-capacity
    ///          containers (LinearSearchMap) whose capacity has to be provided at construction time. The capacity
    ///          equals the number of service-elements (events + fields) of the service-instance, which is known
    ///          up-front. This makes the memory footprint of ServiceDataStorage deterministic and calculable without a
    ///          simulation run.
    /// \param number_of_events_and_fields maximum number of events + fields, that will be stored in events_ and
    ///        events_metainfo_.
    /// \param resource memory-resource to be used for the (single, up-front) allocation of the containers.
    ServiceDataStorage(const std::size_t number_of_events_and_fields,
                       score::memory::shared::ManagedMemoryResource& resource)
        : events_(number_of_events_and_fields, resource),
          events_metainfo_(number_of_events_and_fields, resource),
          skeleton_pid_{impl::GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa).GetPid()},
          skeleton_uid_{os::Unistd::instance().getuid()}
    {
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventDataStorageMap events_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventMetaInfoMap events_metainfo_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    pid_t skeleton_pid_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    uid_t skeleton_uid_;
};

/// \brief Analytically calculates the exact number of bytes a ServiceDataStorage (the data shm-object) occupies.
/// \details This is used by SkeletonMemoryManager, but located next to ServiceDataStorage so that the layout-dependent
///          size algorithm stays coupled to the data structure it reasons about. It does NOT allocate any memory nor
///          construct a ServiceDataStorage; the size is derived purely from the (fixed) container capacities.
///          The result is exact (not just a bound): since all our allocations happen strictly sequentially (single
///          threaded, on a (monotonic) shared-memory resource which always starts allocating at a std::max_align_t
///          aligned location) and we know the size/alignment/order of every individual allocation performed by the
///          real construction, we can reconstruct the exact same sequence of allocations here and compute the exact
///          alignment-padding between them (see score::memory::shared::CalculateAlignedSizeOfSequence()).
/// \param event_and_fields_size_info per service-element sizing information (Size() being the exact size, in bytes,
///        of the raw slot-array that will be allocated for the service-element; Alignment() being its required
///        alignment). The caller (SkeletonMemoryManager) is responsible for computing these values, since they
///        depend on whether the service-element is a typed or a generic (type-erased) event/field:
///        - typed events allocate exactly number_of_slots * sizeof(SampleType) bytes, aligned to alignof(SampleType)
///          (see SkeletonMemoryManager::CreateEventDataInCreatedSharedMemory()).
///        - generic events allocate number_of_slots * sample_size bytes rounded up to a whole number of
///          std::max_align_t elements, aligned to alignof(std::max_align_t) (see
///          SkeletonMemoryManager::CreateGenericEventDataInCreatedSharedMemory()).
///        The size of the span equals the number of service-elements (events + fields), which is the fixed capacity
///        the ServiceDataStorage containers are constructed with.
/// \return the exact number of bytes needed for the data shm-object.
std::size_t CalculateServiceDataStorageShmSize(
    score::cpp::span<const score::memory::DataTypeSizeInfo> event_and_fields_size_info);

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_STORAGE_H
