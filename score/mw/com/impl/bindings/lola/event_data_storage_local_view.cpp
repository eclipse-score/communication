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
#include "score/mw/com/impl/bindings/lola/event_data_storage_local_view.h"

namespace score::mw::com::impl::lola
{

EventDataStorageLocalView::EventDataStorageLocalView(const EventDataStorage& event_data_storage,
                                                     memory::DataTypeSizeInfo data_type_size_info)
    : data_type_size_info_{data_type_size_info}
{
    auto* const start = event_data_storage.GetTypeErasedDataSlotsStart();
    auto* const end = event_data_storage.GetTypeErasedDataSlotsEnd();
    event_data_storage_ = score::cpp::span<std::byte>(start, static_cast<std::size_t>(end - start));
}

void* EventDataStorageLocalView::GetTypeErasedDataSlot(SlotIndexType index) const
{
    const auto data_slot_offset = static_cast<std::size_t>(index) * data_type_size_info_.Size();
    const auto data_slot_end_offset = data_slot_offset + data_type_size_info_.Size();

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        data_slot_end_offset <= event_data_storage_.size(),
        "Accessing a type-erased data slot outside of the bounds of the EventDataStorageLocalView!");

    // This is our low-level data storage, where we work on type-erased data, thus pointer arithmetic can't be avoided.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) see above
    return event_data_storage_.data() + data_slot_offset;
}
}  // namespace score::mw::com::impl::lola
