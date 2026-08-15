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
#include "score/mw/com/impl/initialize_sample_callback.h"

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief Container for storing the actual data of a LoLa event or field within shared-memory.
///
/// \details This container will be accessed in parallel by multiple threads. The access must be synchronized via the
/// EventDataControl block. The idea is that a producer first needs to claim an event slot, then change the data within
/// the storage and then mark the slot as ready (similar for a consumer). This enables us cache optimized access of
/// these data structures. The overall contract will be abstracted for the end-user anyhow, so the separation into two
/// classes should be no problem.
/// The EventDataStorage is type-erased. Because it is located at the binding level. A binding just stores/moves
/// byte-streams. It doesn't need type-information.
/// The storage slots are allocated and (optionally) initialized at construction. The calling layer, which knows, how to
/// type-correctly initialize a slot can hand over a callable, which does the correct initialization.
class EventDataStorage final
{
  public:
    /// \param initialize_sample_callback Optional callback used to initialize (default-construct) every slot right
    ///        away during construction, by calling the callback once for each slot. EventDataStorage itself is
    ///        type-erased, but in the end strongly typed elements are stored. Since EventDataStorage itself has no
    ///        type information, it cannot initialize the slots itself. Therefore, an upper layer with type knowledge
    ///        hands over a callback, which does the correct initialization. The callback is optional, since some
    ///        callers don't have type knowledge (e.g. GenericSkeletonEvent) or don't need type-correct
    ///        initialization (e.g. a simulation-only call solely used to calculate the required shared-memory size).
    EventDataStorage(memory::shared::ManagedMemoryResource& resource,
                     SlotIndexType number_of_slots,
                     memory::DataTypeSizeInfo event_sample_size_info,
                     const std::optional<InitializeSampleCallback>& initialize_sample_callback = std::nullopt);

    ~EventDataStorage();

    /// \brief EventDataStorage is neither copyable nor movable.
    /// \details It manages a raw memory allocation (type_erased_data_slots_) whose lifetime is tied to this
    ///          particular instance (see dtor). It is always constructed in-place within shared memory (via
    ///          ManagedMemoryResource::construct()) and only ever accessed via reference/pointer (see e.g.
    ///          SkeletonMemoryManager, ServiceDataStorage), so there is no need for copy/move semantics.
    EventDataStorage(const EventDataStorage&) = delete;
    EventDataStorage& operator=(const EventDataStorage&) = delete;
    EventDataStorage(EventDataStorage&&) = delete;
    EventDataStorage& operator=(EventDataStorage&&) = delete;

    /// \brief Returns a pointer to the type-erased data slot at the given index.
    /// \details This access also does a complete bounds-check to verify that the returned raw-pointer is within the
    ///          bounds as well as the end-address (returned pointer plus data_size).
    /// \param data_size The size of the data slot. This is used to verify, that the callers size expectation matches
    ///        the size of the event data type, the EventDataStorage was constructed with.
    /// @return A pointer to the type-erased data slot.
    void* GetTypeErasedDataSlot(SlotIndexType index, size_t data_size) const;

    SlotIndexType GetNumberOfSlots() const;

  private:
    /// \brief Initializes the (type erased) slots, by calling the callback for each slot.
    /// \details Called internally from the constructor, so that construction and initialization of the slots happen
    ///          as one atomic step.
    /// \param initialization_callback The callback to be called for each slot, which has a type erased signature.
    void InitializeSlots(const InitializeSampleCallback& initialization_callback);

    SlotIndexType number_of_slots_;
    memory::DataTypeSizeInfo sample_size_info_;
    memory::shared::ManagedMemoryResource& memory_resource_;

    memory::shared::OffsetPtr<std::byte> type_erased_data_slots_;

    /// size of type_erased_data_slots_ storage in bytes. This is equal to number_of_slots_ * sample_size_info_.Size()
    std::size_t type_erased_data_slots_storage_size_in_bytes_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_STORAGE_H
