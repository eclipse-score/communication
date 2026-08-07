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

#include "score/memory/shared/pointer_arithmetic_util.h"

#include <cstddef>

namespace score::mw::com::impl::lola
{

namespace
{

// All alignments occurring within the shm-objects are <= alignof(std::max_align_t). The SharedMemoryResource is a
// monotonic (bump) allocator: the bytes it accounts for a single allocation are the requested size plus the padding
// needed to bring the current offset to the requested alignment. By rounding every individual allocation up to
// alignof(std::max_align_t) (via the shared CalculateAlignedSize() utility) we keep the running offset max-aligned and
// therefore obtain a size that is guaranteed to be sufficient (it is exact whenever the allocated sizes are multiples
// of the involved alignment, which is the common case).
constexpr std::size_t kMaxAlign = alignof(std::max_align_t);

}  // namespace

std::size_t CalculateServiceDataStorageShmSize(
    const score::cpp::span<const ServiceElementDataStorageSizeInfo> service_elements_size_info)
{
    // The number of service-elements determines the (fixed) capacity of the two LinearSearchMaps within the
    // ServiceDataStorage. It equals the number of sizing entries handed over.
    const auto number_of_service_elements = service_elements_size_info.size();

    // (1) The ServiceDataStorage object itself (including the inline bookkeeping of its two LinearSearchMaps).
    std::size_t total_size = memory::shared::CalculateAlignedSize(sizeof(ServiceDataStorage), kMaxAlign);

    // (2) The two backing arrays of the LinearSearchMaps (allocated once, with capacity == number_of_service_elements).
    total_size += memory::shared::CalculateAlignedSize(
        number_of_service_elements * sizeof(ServiceDataStorage::EventDataStorageMap::value_type), kMaxAlign);
    total_size += memory::shared::CalculateAlignedSize(
        number_of_service_elements * sizeof(ServiceDataStorage::EventMetaInfoMap::value_type), kMaxAlign);

    // The size of the EventDataStorage control structure (a DynamicArray) is independent of the concrete sample-type
    // (it only holds an offset-pointer, an allocator and two size_t members).
    const std::size_t event_data_storage_object_size =
        memory::shared::CalculateAlignedSize(sizeof(EventDataStorage<std::max_align_t>), kMaxAlign);

    // (3) For each event/field: the EventDataStorage object plus its raw slot-array.
    // The slot-array size mirrors exactly what the real construction allocates: number_of_slots contiguous slots, each
    // of aligned_slot_size bytes (the caller has already padded the per-slot size to the slot's alignment). The
    // resulting allocation is finally rounded up to a multiple of alignof(std::max_align_t) (the real code allocates it
    // as an array of std::max_align_t elements).
    for (const auto& service_element : service_elements_size_info)
    {
        total_size += event_data_storage_object_size;
        total_size += memory::shared::CalculateAlignedSize(
            service_element.number_of_slots * service_element.aligned_slot_size, kMaxAlign);
    }

    return total_size;
}

}  // namespace score::mw::com::impl::lola
