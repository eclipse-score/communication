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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COHERENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COHERENT_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"

#include "score/containers/dynamic_array.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief EventDataControlCoherent encapsulates the overall control information for one event configured with
/// CoherencyType::kCoherent. It is stored in Shared Memory.
///
/// \details This is the CoherencyType::kCoherent counterpart of EventDataControl (which is used for
/// CoherencyType::kNonCoherent). Since the caller guarantees (e.g. via activity-chain scheduling) that the producer
/// and all of its consumers of a kCoherent event never execute concurrently, the CONTROL segment layout can be much
/// simpler:
/// - state_slots_ only carries a CoherentEventSlotStatus (a plain 4 byte timestamp) per slot instead of the 8 byte
///   atomic EventSlotStatus (timestamp + reference-count) used for kNonCoherent events.
/// - newest_index_ additionally tracks the index of the slot holding the most recently published sample, allowing
///   producer and consumers to avoid a full slot-search (see EventControlCoherent / Nautilus_LoLa.md for details).
///
/// This is a data-only class, just like EventDataControl. All behaviour is added via
/// ProviderEventDataControlCoherentLocalView / ConsumerEventDataControlCoherentLocalView.
class EventDataControlCoherent
{
  public:
    using EventControlSlots =
        containers::DynamicArray<CoherentControlSlotType,
                                 memory::shared::PolymorphicOffsetPtrAllocator<CoherentControlSlotType>>;

    EventDataControlCoherent(const SlotIndexType max_slots, score::memory::shared::ManagedMemoryResource& resource)
        : state_slots_{max_slots, resource}, newest_index_{}
    {
    }

    EventControlSlots state_slots_;

    /// \brief Index of the slot holding the most recently published sample, or an empty optional if no sample has
    /// been published yet.
    ///
    /// \details This is a plain (non-atomic) std::optional: as producer and consumers of a kCoherent event never run
    /// concurrently, no synchronization is required to access it.
    std::optional<SlotIndexType> newest_index_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_COHERENT_H
