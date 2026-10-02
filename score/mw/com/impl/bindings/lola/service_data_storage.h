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
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"
#include "score/mw/com/impl/bindings/lola/event_meta_info.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/linear_search_map.h"
#include "score/mw/com/impl/initialize_sample_callback.h"
#include "score/mw/com/impl/runtime.h"

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"
#include "score/os/unistd.h"

#include <score/span.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

class ServiceDataStorage
{
  public:
    /// \brief associative container mapping a service-element (event/field) to the raw storage of its event-data slots.
    /// \details The value-type of the map is a pointer to the storage of the type-erased event-data slots.
    ///          The OffsetPtr points to a EventDataStorage, which gets created by events/fields, when calling
    ///          Skeleton::Register()!
    using EventDataStorageMap = LinearSearchMap<ElementFqId, memory::shared::OffsetPtr<EventDataStorage>>;
    /// \brief associative container mapping a service-element (event/field) to its (type-erased) meta-information.
    using EventMetaInfoMap = LinearSearchMap<ElementFqId, EventMetaInfo>;

    /// \brief Ctor for the ServiceDataStorage with a given memory resource to be used for internal storage allocation.
    /// \details ServiceDataStorage no longer uses dynamically allocating map-types. Instead, it uses fixed-capacity
    ///          containers (LinearSearchMap) whose capacity has to be provided at construction time. The capacity
    ///          equals the number of service-elements (events + fields) of the service-instance, which is known
    ///          up-front. This makes the memory footprint of ServiceDataStorage deterministic and calculable without a
    ///          simulation run.
    /// \param number_of_events_and_fields maximum number of events + fields, that will be stored in events_ and
    ///        events_metainfo_.
    /// \param resource memory-resource to be used for the (single, up-front) allocation of the containers.
    ServiceDataStorage(const std::size_t number_of_events_and_fields, memory::shared::ManagedMemoryResource& resource)
        : events_(number_of_events_and_fields, resource),
          events_metainfo_(number_of_events_and_fields, resource),
          skeleton_pid_{impl::GetBindingRuntime<lola::IRuntime>(BindingType::kLoLa).GetPid()},
          skeleton_uid_{os::Unistd::instance().getuid()},
          allocator_{resource},
          resource_ptr_{resource.getMemoryResourceProxy()}
    {
    }

    /// \brief Constructs a new (type-erased) EventDataStorage for the given service-element (event/field) on the
    ///        very same memory-resource this ServiceDataStorage was constructed on, and registers it (together with
    ///        its meta-information) within this ServiceDataStorage.
    /// \param element_fq_id Full qualified ID of the service-element (event/field) the EventDataStorage belongs to.
    /// \param number_of_slots Number of (type-erased) event-data slots to allocate.
    /// \param sample_size_info Size/alignment of a single sample of the service-element's datatype.
    /// \param initialize_sample_callback Optional callback used to initialize every slot right away during
    ///        construction (see EventDataStorage's ctor for details).
    /// \return A reference to the newly created EventDataStorage.
    EventDataStorage& AddEvent(
        const ElementFqId element_fq_id,
        const SlotIndexType number_of_slots,
        const memory::DataTypeSizeInfo sample_size_info,
        const std::optional<InitializeSampleCallback>& initialize_sample_callback = std::nullopt);

    /// \brief Returns the EventDataStorage registered for the given service-element.
    /// \details Terminates if no EventDataStorage is registered for element_fq_id (see AddEvent()). Every valid
    ///          caller is expected to only query previously registered service-elements.
    /// \param element_fq_id Full qualified ID of the service-element (event/field).
    /// \return A reference to the registered EventDataStorage.
    EventDataStorage& GetEventDataStorage(const ElementFqId element_fq_id) noexcept;
    const EventDataStorage& GetEventDataStorage(const ElementFqId element_fq_id) const noexcept;

    /// \brief Returns the EventMetaInfo registered for the given service-element.
    /// \details Terminates if no EventMetaInfo is registered for element_fq_id (see AddEvent()). Every valid caller
    ///          is expected to only query previously registered service-elements.
    /// \param element_fq_id Full qualified ID of the service-element (event/field).
    /// \return A reference to the registered EventMetaInfo.
    const EventMetaInfo& GetEventMetaInfo(const ElementFqId element_fq_id) const noexcept;

    /// \brief Returns the pid of the skeleton-process that owns this ServiceDataStorage.
    pid_t GetSkeletonPid() const noexcept;

    /// \brief Updates the pid of the skeleton-process that owns this ServiceDataStorage.
    /// \details Used after a (partial-)restart, where the skeleton-process got a new pid, which has to be updated
    ///          in the re-opened/shared ServiceDataStorage (see
    ///          SkeletonMemoryManager::GetServiceDataStorageSkeletonSide()).
    void UpdateSkeletonPid(const pid_t pid) noexcept;

    /// \brief Returns the uid of the skeleton-process that owns this ServiceDataStorage.
    uid_t GetSkeletonUid() const noexcept;

    /// \brief Updates the uid of the skeleton-process that owns this ServiceDataStorage.
    void UpdateSkeletonUid(const uid_t uid) noexcept;

  private:
    EventDataStorageMap events_;
    EventMetaInfoMap events_metainfo_;
    pid_t skeleton_pid_;
    uid_t skeleton_uid_;

    memory::shared::PolymorphicOffsetPtrAllocator<EventDataStorage> allocator_;
    const memory::shared::MemoryResourceProxy* resource_ptr_;
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
/// \param event_and_fields_size_info per service-element sizing information: the number of (type-erased) slots plus
///        the size/alignment of a single slot. The caller (SkeletonMemoryManager) is responsible for providing these
///        values (see SkeletonMemoryManager::CalculateDataShmResourceStorageSize()).
///        The size of the span equals the number of service-elements (events + fields), which is the fixed capacity
///        the ServiceDataStorage containers are constructed with.
/// \return the exact number of bytes needed for the data shm-object.
std::size_t CalculateServiceDataStorageShmSize(
    score::cpp::span<const EventDataStorageSizeInfo> event_and_fields_size_info);

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SKELETON_DATA_STORAGE_H
