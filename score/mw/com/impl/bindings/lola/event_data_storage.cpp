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
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"

#include "score/language/safecpp/safe_math/safe_math.h"

#include <score/assert.hpp>

#include <functional>
#include <tuple>

namespace score::mw::com::impl::lola
{

EventDataStorage::EventDataStorage(memory::shared::ManagedMemoryResource& resource,
                                   SlotIndexType number_of_slots,
                                   memory::DataTypeSizeInfo event_sample_size_info,
                                   const std::optional<InitializeSampleCallback>& initialize_sample_callback)
    : number_of_slots_(number_of_slots),
      sample_size_info_(event_sample_size_info),
      memory_resource_(resource),
      type_erased_data_slots_(nullptr),
      type_erased_data_slots_storage_size_in_bytes_(0)
{
    const auto storage_bytes_needed_result =
        safe_math::Multiply<safe_math::ReturnMode::kReturnResultOnError, std::size_t>(number_of_slots,
                                                                                      event_sample_size_info.Size());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        storage_bytes_needed_result.has_value(),
        "Overflow while calculating the total size of the raw event-data slot-array.");
    const auto storage_bytes_needed = storage_bytes_needed_result.value();

    // The alignment used here must match exactly the alignment CalculateServiceDataStorageShmSize() assumes for this
    // allocation (see service_data_storage.cpp), i.e. event_sample_size_info.Alignment(). Using a different (e.g.
    // hardcoded, stricter) alignment here would make the analytically calculated shm-size wrong (too small).
    void* const type_erased_data_slots_start =
        memory_resource_.allocate(storage_bytes_needed, event_sample_size_info.Alignment());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(nullptr != type_erased_data_slots_start);
    type_erased_data_slots_ = static_cast<std::byte*>(type_erased_data_slots_start);
    type_erased_data_slots_storage_size_in_bytes_ = storage_bytes_needed;

    // Construction and initialization of the slots is done as one step: if the caller handed over an initializer,
    // use it to initialize (e.g. default-construct) every slot right away. The callback might be empty if the
    // binding independent layer doesn't need to initialize the slots (e.g. for a generic event or a simulation-only
    // call solely used to calculate the required shared-memory size).
    if (initialize_sample_callback.has_value())
    {
        InitializeSlots(*initialize_sample_callback);
    }
}

// Deviation of MISRA RULE-18-5-1: codeql::misra_deviation_next_line(destructor-contract-violation-terminate)
EventDataStorage::~EventDataStorage() noexcept
{
    if (type_erased_data_slots_ != nullptr)
    {
        memory_resource_.deallocate(type_erased_data_slots_.get(), type_erased_data_slots_storage_size_in_bytes_);
    }
}

std::byte* EventDataStorage::GetTypeErasedDataSlotsStart() const
{
    return type_erased_data_slots_.get();
}

std::byte* EventDataStorage::GetTypeErasedDataSlotsEnd() const
{
    if (type_erased_data_slots_storage_size_in_bytes_ == 0U)
    {
        return GetTypeErasedDataSlotsStart();
    }

    // A one-past-the-end pointer is a legal, but special, address: forming it is well-defined, but there is no
    // guarantee that an object of the pointed-to type actually starts there (see
    // score/memory/shared/design/offset_ptr_problems.md, "One-past-the-end-iterators"). Therefore we must not form an
    // OffsetPtr<std::byte> at that address and call get() on it directly: that would additionally check that a
    // complete std::byte (i.e. one-past-the-end address + 1) still fits within the memory region, which spuriously
    // fails whenever the data slots storage happens to end exactly at the boundary of the shared-memory region (a
    // common case, since this allocation is usually the last/biggest one in the region).
    // Instead (mirroring the established pattern used e.g. by NonRelocatableVector::GetPastTheEndIterator()), we do a
    // full, regular (i.e. start- and end-) bounds-check on the *last valid byte* of the storage via
    // OffsetPtr<std::byte>::get(). Since the storage is contiguous, this implicitly already proves that
    // last_valid_byte_address + 1 (== start + size, i.e. exactly the one-past-the-end address we want to return) is
    // still within bounds. We can therefore safely compute the final, one-past-the-end address via plain pointer
    // arithmetic on that already-validated raw pointer, without triggering another (incorrect) bounds-check.
    const auto last_valid_byte_offset_ptr =
        type_erased_data_slots_ +
        decltype(type_erased_data_slots_)::difference_type(type_erased_data_slots_storage_size_in_bytes_ - 1U);
    auto* const last_valid_byte_raw_ptr = last_valid_byte_offset_ptr.get();
    // One-past-the-end is guaranteed to be within bounds by the get() call above, so plain pointer arithmetic (which
    // performs no further bounds-check) is safe here.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) see rationale above
    return last_valid_byte_raw_ptr + 1;
}

void EventDataStorage::InitializeSlots(const InitializeSampleCallback& initialization_callback)
{
    // Retrieve 1st/last slot raw-pointers from OffsetPtrs, which includes bounds-checking.
    auto* first_slot_raw_ptr = type_erased_data_slots_.get();
    const auto last_slot_offset = sample_size_info_.Size() * (number_of_slots_ - 1U);
    auto last_slot_ptr = type_erased_data_slots_ + decltype(type_erased_data_slots_)::difference_type(last_slot_offset);
    auto* last_slot_raw_ptr = last_slot_ptr.get();

    // This is our low-level data storage, where we work on type-erased data, thus pointer arithmetic can't be avoided.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) see above
    for (auto* current_slot_raw_ptr = first_slot_raw_ptr; current_slot_raw_ptr <= last_slot_raw_ptr;
         current_slot_raw_ptr += sample_size_info_.Size())
    {
        std::invoke(initialization_callback, current_slot_raw_ptr);
    }
}

void AddEventDataStorageShmSizeAllocation(std::vector<score::memory::DataTypeSizeInfo>& allocation_sequence,
                                          memory::DataTypeSizeInfo event_sample_array_size_info)
{
    std::ignore = allocation_sequence.emplace_back(sizeof(EventDataStorage), alignof(EventDataStorage));
    std::ignore =
        allocation_sequence.emplace_back(event_sample_array_size_info.Size(), event_sample_array_size_info.Alignment());
}

}  // namespace score::mw::com::impl::lola
