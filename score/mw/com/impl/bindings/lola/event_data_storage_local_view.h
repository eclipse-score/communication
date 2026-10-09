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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_STORAGE_LOCAL_VIEW_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_STORAGE_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/event_data_storage.h"

#include <score/span.hpp>
#include <cstddef>

namespace score::mw::com::impl::lola
{
class EventDataStorageLocalView final
{
  public:
    using LocalEventDataStorage = score::cpp::span<std::byte>;

    /// \brief Constructs a local view of the given EventDataStorage.
    /// \details data_type_size_info is stored internally and used for two things: GetTypeErasedDataSlot() member func
    /// relies on it, when doing index calculation and verifying the access.
    /// \param event_data_storage The EventDataStorage to create a local view of.
    /// \param data_type_size_info The size and alignment information of the event data type stored in the
    /// EventDataStorage.
    explicit EventDataStorageLocalView(const EventDataStorage& event_data_storage,
                                       memory::DataTypeSizeInfo data_type_size_info);

    EventDataStorageLocalView(const EventDataStorageLocalView&) = delete;
    EventDataStorageLocalView& operator=(const EventDataStorageLocalView&) = delete;

    EventDataStorageLocalView(EventDataStorageLocalView&&) = default;
    EventDataStorageLocalView& operator=(EventDataStorageLocalView&&) = default;

    /// \brief Returns a pointer to the type-erased data slot at the given index.
    /// \details This accessor uses the stored data_type_size_info (provided to ctor) to calculate the offset of the
    /// requested slot and verify that the access is within bounds.
    /// \return A pointer to the type-erased data slot.
    void* GetTypeErasedDataSlot(SlotIndexType index) const;

  private:
    LocalEventDataStorage event_data_storage_;
    memory::DataTypeSizeInfo data_type_size_info_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_STORAGE_LOCAL_VIEW_H
