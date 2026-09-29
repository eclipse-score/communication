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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SLOT_ALLOCATION_CONTROL_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SLOT_ALLOCATION_CONTROL_H

#include "score/mw/com/impl/bindings/someip/event_data_storage.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace score::mw::com::impl::someip
{

/// \brief Tracks which sample slots of an event are currently handed out to the user.
///
/// \details The owning SkeletonEvent holds this as a member, and every SampleAllocateePtr handed out by that
///          event refers to it (not to the SkeletonEvent) in order to return its slot. Keeping the slot state in its
///          own type is what avoids a dependency cycle between SkeletonEvent and SampleAllocateePtr.
///
class SlotAllocationControl final
{
  public:
    SlotAllocationControl() = default;

    /// \brief (Re-)initializes the control for the given number of slots, marking all of them as free.
    void Reset(std::size_t number_of_slots);

    /// \brief Drops all slots, so that no slot can be allocated until the next Reset().
    void Clear() noexcept;

    /// \brief Claims a free slot.
    /// \return The index of the claimed slot, or an empty optional if all slots are currently in use.
    std::optional<SlotIndexType> AllocateSlot() noexcept;

    /// \brief Returns a slot which was handed out by AllocateSlot() so that it can be reused.
    void DiscardSlot(SlotIndexType slot_index) noexcept;

    std::size_t GetNumberOfSlots() const noexcept;

  private:
    std::vector<bool> slot_in_use_{};
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SLOT_ALLOCATION_CONTROL_H
