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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_STORAGE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_STORAGE_H

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/mw/com/impl/bindings/lola/control_slot_types.h"

#include <cstddef>

namespace score::mw::com::impl::lola
{

/// \brief Container for storing the actual data of a LoLa event (resp. field) within shared-memory.
///
/// \details This container will be accessed in parallel by multiple threads. The access must be synchronized via the
/// EventDataControl block. The idea is that a producer first needs to claim an event slot, then change the data within
/// the storage and then mark the slot as ready (similar for a consumer). This enables us cache optimized access of
/// these data structures. The overall contract will be abstracted for the end-user anyhow, so the separation into two
/// classes should be no problem.
class EventDataStorage final
{
  public:
    EventDataStorage(memory::shared::ManagedMemoryResource& resource,
                     SlotIndexType number_of_slots,
                     memory::DataTypeSizeInfo event_sample_size_info);

    ~EventDataStorage();

    /// \brief Returns a pointer to the type-erased data slot at the given index.
    /// \details This access also does a complete bounds-check to verify that the returned raw-pointer is within the
    ///          bounds as well as the end-address (returned pointer plus data_size).
    /// \param data_size The size of the data slot. This is used to verify, that the callers size expectation matches
    ///        the size of the event data type, the EventDataStorage was constructed with.
    /// @return A pointer to the type-erased data slot.
    void* GetTypeErasedDataSlot(SlotIndexType index, size_t data_size) const;

    SlotIndexType GetNumberOfSlots() const;

  private:
    SlotIndexType number_of_slots_;
    memory::DataTypeSizeInfo sample_size_info_;
    memory::shared::ManagedMemoryResource& memory_resource_;

    memory::shared::OffsetPtr<std::byte> type_erased_data_slots_;

    /// size of type_erased_data_slots_ storage in bytes. This is equal to number_of_slots_ * sample_size_info_.Size()
    std::size_t type_erased_data_slots_storage_size_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_STORAGE_H
