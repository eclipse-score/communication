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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_EVENT_DATA_STORAGE_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_EVENT_DATA_STORAGE_H

#include "score/memory/data_type_size_info.h"
#include "score/mw/com/impl/initialize_sample_callback.h"

#include <cstddef>
#include <cstdint>

namespace score::mw::com::impl::someip
{

using SlotIndexType = std::uint16_t;

/// \brief Container for storing the actual data of a SOME/IP event (resp. field).
///
/// \details This mirrors lola::EventDataStorage: it is a type-erased, slot based container which only knows the size
///          and alignment of one sample. The difference to the LoLa counterpart is where the slots live: LoLa places
///          them into a shared-memory region managed by a ManagedMemoryResource and addresses them via OffsetPtr,
///          while the SOME/IP binding keeps them in process-local heap memory. Slots are process-local because a
///          SOME/IP provider never hands raw memory to its consumers; it would send the bytes over the transport.
class EventDataStorage final
{
  public:
    EventDataStorage(SlotIndexType number_of_slots, memory::DataTypeSizeInfo event_sample_size_info);

    ~EventDataStorage();

    EventDataStorage(const EventDataStorage&) = delete;
    EventDataStorage(EventDataStorage&&) noexcept = delete;
    EventDataStorage& operator=(const EventDataStorage&) & = delete;
    EventDataStorage& operator=(EventDataStorage&&) & noexcept = delete;

    /// \brief Initializes the (type erased) slots, by calling the callback for each slot.
    /// \details EventDataStorage itself is type-erased. But in the end strongly typed elements are stored. Since
    ///          EventDataStorage itself has no type information, it cannot initialize the slots itself. Therefore, an
    ///          upper layer with type knowledge calls this API handing over a callback, which does the correct
    ///          initialization.
    void InitializeSlots(const InitializeSampleCallback& callback);

    /// \brief Returns a pointer to the type-erased data slot at the given index.
    /// \details This access also does a complete bounds-check to verify that the returned raw-pointer is within the
    ///          bounds as well as the end-address (returned pointer plus data_size).
    /// \param data_size The size of the data slot. This is used to verify, that the callers size expectation matches
    ///        the size of the event data type, the EventDataStorage was constructed with.
    /// \return A pointer to the type-erased data slot.
    void* GetTypeErasedDataSlot(SlotIndexType index, std::size_t data_size) const;

    SlotIndexType GetNumberOfSlots() const noexcept;

  private:
    SlotIndexType number_of_slots_;
    memory::DataTypeSizeInfo sample_size_info_;

    std::byte* type_erased_data_slots_;

    /// size of type_erased_data_slots_ storage in bytes. This is equal to number_of_slots_ * sample_size_info_.Size()
    std::size_t type_erased_data_slots_storage_size_;
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_EVENT_DATA_STORAGE_H
