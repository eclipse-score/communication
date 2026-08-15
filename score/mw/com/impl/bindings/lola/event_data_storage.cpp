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

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <limits>

namespace score::mw::com::impl::lola
{

EventDataStorage::EventDataStorage(memory::shared::ManagedMemoryResource& resource,
                                   SlotIndexType number_of_slots,
                                   memory::DataTypeSizeInfo event_sample_size_info)
    : number_of_slots_(number_of_slots),
      sample_size_info_(event_sample_size_info),
      memory_resource_(resource),
      type_erased_data_slots_(nullptr),
      type_erased_data_slots_storage_size_(0)
{
    // Guard against an overflow when calculating the total number of bytes needed for the raw slot-array. Without
    // this check, an overflowing multiplication would silently wrap around to a much smaller value than what is
    // actually required, leading to an undersized allocation and, subsequently, out-of-bounds accesses when the
    // slots are used (see GetTypeErasedDataSlot()).
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        (event_sample_size_info.Size() == 0U) ||
            (number_of_slots <= (std::numeric_limits<std::size_t>::max() / event_sample_size_info.Size())),
        "Overflow while calculating the total size of the raw event-data slot-array.");
    const auto storage_bytes_needed = number_of_slots * event_sample_size_info.Size();

    // The alignment used here must match exactly the alignment CalculateServiceDataStorageShmSize() assumes for this
    // allocation (see service_data_storage.cpp), i.e. event_sample_size_info.Alignment(). Using a different (e.g.
    // hardcoded, stricter) alignment here would make the analytically calculated shm-size wrong (too small).
    void* const type_erased_data_slots_start =
        memory_resource_.allocate(storage_bytes_needed, event_sample_size_info.Alignment());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(nullptr != type_erased_data_slots_start);
    type_erased_data_slots_ = static_cast<std::byte*>(type_erased_data_slots_start);
    type_erased_data_slots_storage_size_ = storage_bytes_needed;
}

EventDataStorage::~EventDataStorage()
{
    if (type_erased_data_slots_ != nullptr)
    {
        memory_resource_.deallocate(type_erased_data_slots_.get(), type_erased_data_slots_storage_size_);
    }
}

void EventDataStorage::InitializeSlots(InitializeSampleCallback callback)
{
    // Retrieve 1st/last slot raw-pointers from OffsetPtrs, which includes bounds-checking.
    auto* first_slot_raw_ptr = type_erased_data_slots_.get();
    const auto last_slot_offset = sample_size_info_.Size() * (number_of_slots_ - 1U);
    auto last_slot_ptr = type_erased_data_slots_ + decltype(type_erased_data_slots_)::difference_type(last_slot_offset);
    auto* last_slot_raw_ptr = last_slot_ptr.get();

    for (auto* current_slot_raw_ptr = first_slot_raw_ptr; current_slot_raw_ptr <= last_slot_raw_ptr;
         current_slot_raw_ptr += sample_size_info_.Size())
    {
        callback(current_slot_raw_ptr);
    }
}

void* EventDataStorage::GetTypeErasedDataSlot(SlotIndexType index, size_t data_size) const
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(index < number_of_slots_);

    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(data_size == sample_size_info_.Size());
    const auto element_offset = data_size * index;

    // we apply the required bounds-checking:
    // Verify, that the start of the type-erased storage is still within bounds
    auto* const slots_start_address = type_erased_data_slots_.get();
    // Verify, that the complete slot to be accessed in the type-erased storage is still within bounds.
    const auto slot_last_byte_offset_ptr =
        type_erased_data_slots_ + decltype(type_erased_data_slots_)::difference_type(element_offset + data_size - 1U);
    score::cpp::ignore = slot_last_byte_offset_ptr.get();

    // In our architecture we have a one-to-one mapping between pointers and integral values.
    // The preconditions above guarantee that element_address will always point inside type_erased_data_slots_.
    // Therefore, casting between integers and pointers is well-defined in this case.
    // NOLINTNEXTLINE(score-banned-function) see above
    auto* const element_address = memory::shared::AddOffsetToPointer(slots_start_address, element_offset);

    return element_address;
}

SlotIndexType EventDataStorage::GetNumberOfSlots() const
{
    return number_of_slots_;
}

}  // namespace score::mw::com::impl::lola
