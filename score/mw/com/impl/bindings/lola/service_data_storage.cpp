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
#include "score/mw/com/impl/bindings/lola/service_data_storage.h"

#include "score/mw/com/impl/bindings/lola/event_data_storage.h"

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <cstddef>
#include <vector>

namespace score::mw::com::impl::lola
{

std::size_t CalculateServiceDataStorageShmSize(
    const score::cpp::span<const score::memory::DataTypeSizeInfo> event_and_fields_size_info)
{
    // The number of events + fields determines the (fixed) capacity of the two LinearSearchMaps within the
    // ServiceDataStorage. It equals the number of sizing entries handed over.
    const auto number_of_events_and_fields = event_and_fields_size_info.size();

    // The real construction of a ServiceDataStorage and the EventDataStorage of each of its events/fields performs a
    // fixed, deterministic sequence of allocations from the (strictly monotonic) shared-memory resource, which itself
    // always starts allocating at a std::max_align_t aligned location. Since we know the exact size/alignment of
    // every single one of these allocations and the exact order in which they happen, we can reconstruct the exact
    // sequence here and let memory::shared::CalculateAlignedSizeOfSequence() compute the exact (not just worst-case)
    // total size, taking into account the exact alignment-padding between consecutive allocations.
    std::vector<score::memory::DataTypeSizeInfo> allocation_sequence{};

    // (1) The ServiceDataStorage object itself (including the inline bookkeeping of its two LinearSearchMaps).
    allocation_sequence.emplace_back(sizeof(ServiceDataStorage), alignof(ServiceDataStorage));

    // (2) The two allocated arrays of the LinearSearchMaps (allocated once, with capacity ==
    // number_of_events_and_fields).
    allocation_sequence.emplace_back(
        number_of_events_and_fields * sizeof(ServiceDataStorage::EventDataStorageMap::value_type),
        alignof(ServiceDataStorage::EventDataStorageMap::value_type));
    allocation_sequence.emplace_back(
        number_of_events_and_fields * sizeof(ServiceDataStorage::EventMetaInfoMap::value_type),
        alignof(ServiceDataStorage::EventMetaInfoMap::value_type));

    // (3) For each event/field (in the exact order it gets registered/offered): the EventDataStorage object plus its
    // raw slot-array. The exact size/alignment of the slot-array (which differs between typed and generic
    // events/fields, see SkeletonMemoryManager::CreateEventDataInCreatedSharedMemory() resp.
    // CreateGenericEventDataInCreatedSharedMemory()) is provided by the caller.

    // The size/alignment of the EventDataStorage control structure (a DynamicArray) is independent of the concrete
    // sample-type (it only holds an offset-pointer, an allocator and two size_t members).
    constexpr std::size_t event_data_storage_object_size = sizeof(EventDataStorage<std::max_align_t>);
    constexpr std::size_t event_data_storage_object_alignment = alignof(EventDataStorage<std::max_align_t>);
    for (const auto& service_element : event_and_fields_size_info)
    {
        allocation_sequence.emplace_back(event_data_storage_object_size, event_data_storage_object_alignment);
        allocation_sequence.emplace_back(service_element.Size(), service_element.Alignment());
    }

    return score::memory::shared::CalculateAlignedSizeOfSequence(allocation_sequence);
}

}  // namespace score::mw::com::impl::lola
