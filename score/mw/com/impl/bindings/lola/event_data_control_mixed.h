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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_MIXED_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_MIXED_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"

#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief EventDataControlMixed encapsulates the overall control information for one event configured with
/// CoherencyType::kMixed. It is stored in Shared Memory.
///
/// \details This is the CoherencyType::kMixed counterpart of EventDataControl (which is used for
/// CoherencyType::kNonCoherent) / EventDataControlCoherent (which is used for CoherencyType::kCoherent). Unlike
/// CoherencyType::kCoherent, the CONTROL segment layout for CoherencyType::kMixed is identical to
/// CoherencyType::kNonCoherent (the same 8 byte atomic EventSlotStatus per slot, allowing non-coherent consumers to
/// access it exactly as for a CoherencyType::kNonCoherent event), which is why data_control_ embeds a plain
/// EventDataControl instead of duplicating its fields.
///
/// In addition, newest_index_ tracks the index of the slot holding the most recently published sample, allowing a
/// coherent consumer of a CoherencyType::kMixed event to avoid a full slot-search (see Nautilus_LoLa.md for
/// details), the same way CoherencyType::kCoherent's newest_index_ does for ProviderEventDataControlCoherentLocalView
/// / ConsumerEventDataControlCoherentLocalView.
///
/// This is a data-only class, just like EventDataControl / EventDataControlCoherent. All behaviour is added via
/// dedicated local views.
class EventDataControlMixed
{
  public:
    EventDataControlMixed(const SlotIndexType max_slots, score::memory::shared::ManagedMemoryResource& resource)
        : data_control_{max_slots, resource}, newest_index_{}
    {
    }

    EventDataControl data_control_;

    /// \brief Index of the slot holding the most recently published sample, or an empty optional if no sample has
    /// been published yet.
    ///
    /// \details This is a plain (non-atomic) std::optional: as the producer and the coherent consumers of a
    /// CoherencyType::kMixed event never run concurrently (the non-coherent consumers, which may run concurrently
    /// with the producer, never access newest_index_), no synchronization is required to access it.
    std::optional<SlotIndexType> newest_index_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_DATA_CONTROL_MIXED_H
