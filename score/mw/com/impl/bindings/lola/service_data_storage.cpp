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

#include <score/assert.hpp>

#include <cstddef>
#include <tuple>
#include <vector>

namespace score::mw::com::impl::lola
{

namespace
{

/// \brief Looks up element_fq_id in map and terminates if it isn't registered.
/// \details Shared by GetEventDataStorage()/GetEventMetaInfo() (both overloads) to avoid duplicating the
///          find-or-terminate logic. MapType is deduced as either EventDataStorageMap or EventMetaInfoMap (const or
///          non-const), so the const-correctness of the returned reference follows from MapType's constness.
/// \return A reference to the mapped_type registered for element_fq_id.
template <typename MapType>
auto& GetOrTerminate(MapType& map, const ElementFqId element_fq_id) noexcept
{
    auto it = map.find(element_fq_id);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(it != map.end(),
                                                "No service-element registered for the given ElementFqId.");
    return it->second;
}

}  // namespace

EventDataStorage& ServiceDataStorage::AddEvent(
    const ElementFqId element_fq_id,
    const SlotIndexType number_of_slots,
    const memory::DataTypeSizeInfo sample_size_info,
    const std::optional<InitializeSampleCallback>& initialize_sample_callback)
{
    // Construction and initialization of the (type-erased) storage slots is done as one step by EventDataStorage's
    // constructor: it initializes the freshly created slots using the initializer handed down from the strongly
    // typed binding independent layer. The callback might be empty if the binding independent layer doesn't need to
    // initialize the slots (e.g. for a generic event or a simulation-only call solely used to calculate the required
    // shared-memory size).
    memory::shared::OffsetPtr<EventDataStorage> data_storage = allocator_.allocate(1);
    std::allocator_traits<decltype(allocator_)>::construct(
        allocator_, data_storage.get(), resource_ptr_, number_of_slots, sample_size_info, initialize_sample_callback);

    auto inserted_data_slots = events_.emplace(
        std::piecewise_construct, std::forward_as_tuple(element_fq_id), std::forward_as_tuple(data_storage));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted_data_slots.second,
                                                "Couldn't register/emplace event-storage in data-section.");

    auto inserted_meta_info = events_metainfo_.emplace(
        std::piecewise_construct, std::forward_as_tuple(element_fq_id), std::forward_as_tuple(sample_size_info));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(inserted_meta_info.second,
                                                "Couldn't register/emplace event-meta-info in data-section.");

    return *data_storage;
}

EventDataStorage& ServiceDataStorage::GetEventDataStorage(const ElementFqId element_fq_id) noexcept
{
    return *GetOrTerminate(events_, element_fq_id);
}

const EventDataStorage& ServiceDataStorage::GetEventDataStorage(const ElementFqId element_fq_id) const noexcept
{
    return *GetOrTerminate(events_, element_fq_id);
}

const EventMetaInfo& ServiceDataStorage::GetEventMetaInfo(const ElementFqId element_fq_id) const noexcept
{
    return GetOrTerminate(events_metainfo_, element_fq_id);
}

pid_t ServiceDataStorage::GetSkeletonPid() const noexcept
{
    return skeleton_pid_;
}

void ServiceDataStorage::UpdateSkeletonPid(const pid_t pid) noexcept
{
    skeleton_pid_ = pid;
}

uid_t ServiceDataStorage::GetSkeletonUid() const noexcept
{
    return skeleton_uid_;
}

void ServiceDataStorage::UpdateSkeletonUid(const uid_t uid) noexcept
{
    skeleton_uid_ = uid;
}

std::size_t CalculateServiceDataStorageShmSize(
    const score::cpp::span<const EventDataStorageSizeInfo> event_and_fields_size_info)
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
    std::ignore = allocation_sequence.emplace_back(sizeof(ServiceDataStorage), alignof(ServiceDataStorage));

    // (2) The two allocated arrays of the LinearSearchMaps (allocated once, with capacity ==
    // number_of_events_and_fields).
    std::ignore = allocation_sequence.emplace_back(
        number_of_events_and_fields * sizeof(ServiceDataStorage::EventDataStorageMap::value_type),
        alignof(ServiceDataStorage::EventDataStorageMap::value_type));
    std::ignore = allocation_sequence.emplace_back(
        number_of_events_and_fields * sizeof(ServiceDataStorage::EventMetaInfoMap::value_type),
        alignof(ServiceDataStorage::EventMetaInfoMap::value_type));

    // (3) For each event/field (in the exact order it gets registered/offered): the EventDataStorage object plus its
    // data-slot-array (type_erased_data_slots_). The number of slots plus the size/alignment of a single slot is
    // provided by the caller; AddEventDataStorageShmSizeAllocation() itself computes the actual slot-array allocation
    // size/alignment needs.
    for (const auto& service_element : event_and_fields_size_info)
    {
        AddEventDataStorageShmSizeAllocation(allocation_sequence, service_element);
    }

    return score::memory::shared::CalculateAlignedSizeOfSequence(allocation_sequence);
}

}  // namespace score::mw::com::impl::lola
