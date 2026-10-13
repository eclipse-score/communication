/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/someip/event_data_storage.h"

#include <score/assert.hpp>

#include <limits>
#include <new>

namespace score::mw::com::impl::someip
{

EventDataStorage::EventDataStorage(const SlotIndexType number_of_slots,
                                   const memory::DataTypeSizeInfo event_sample_size_info)
    : number_of_slots_{number_of_slots},
      sample_size_info_{event_sample_size_info},
      type_erased_data_slots_{nullptr},
      type_erased_data_slots_storage_size_{0U}
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
    if (storage_bytes_needed == 0U)
    {
        return;
    }

    void* const type_erased_data_slots_start =
        ::operator new(storage_bytes_needed, std::align_val_t{event_sample_size_info.Alignment()}, std::nothrow);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(nullptr != type_erased_data_slots_start);
    type_erased_data_slots_ = static_cast<std::byte*>(type_erased_data_slots_start);
    type_erased_data_slots_storage_size_ = storage_bytes_needed;
}

EventDataStorage::~EventDataStorage()
{
    if (type_erased_data_slots_ != nullptr)
    {
        ::operator delete(type_erased_data_slots_,
                          type_erased_data_slots_storage_size_,
                          std::align_val_t{sample_size_info_.Alignment()});
    }
}

void EventDataStorage::InitializeSlots(const InitializeSampleCallback& callback)
{
    for (SlotIndexType slot_index = 0U; slot_index < number_of_slots_; ++slot_index)
    {
        callback(GetTypeErasedDataSlot(slot_index, sample_size_info_.Size()));
    }
}

void* EventDataStorage::GetTypeErasedDataSlot(const SlotIndexType index, const std::size_t data_size) const
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(index < number_of_slots_);
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(data_size == sample_size_info_.Size());

    const auto element_offset = data_size * index;

    // Verify that the complete slot to be accessed is within the bounds of the allocated storage.
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD((element_offset + data_size) <= type_erased_data_slots_storage_size_);
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(nullptr != type_erased_data_slots_);

    // This is our low-level data storage, where we work on type-erased data, thus pointer arithmetic can't be avoided.
    // The preconditions above guarantee that the resulting address stays inside type_erased_data_slots_.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) see above
    return static_cast<void*>(type_erased_data_slots_ + element_offset);
}

SlotIndexType EventDataStorage::GetNumberOfSlots() const noexcept
{
    return number_of_slots_;
}

}  // namespace score::mw::com::impl::someip
