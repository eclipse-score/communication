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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SLOT_INDICATOR_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SLOT_INDICATOR_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <atomic>
#include <cstddef>

namespace score::mw::com::impl::lola
{

/// \brief Helper class which identifies a slot in our "control slot array" using both the slot index and a raw pointer
///        to the element.
/// \details We combine identification by the index of the slot and a (raw) pointer to the slot. Reasoning:
///          Sometimes the index is more appropriate - as we have a 1:1 relation between slot indices in DATA and
///          CONTROL arrays. But the slot access via index might be not as efficient! (Hint: DynamicArrays based
///          on OffsetPtr, which represent our "control slot array", will give you the penalty of bounds-checking, when
///          doing index based access). If the access can be performance critical, the access via the raw-pointer is
///          preferred (and if its safe as the slot has been already bounds-checked.
class ControlSlotIndicator
{
  public:
    ControlSlotIndicator();

    ControlSlotIndicator(SlotIndexType slot_index, ControlSlotType& slot);

    bool IsValid() const;

    SlotIndexType GetIndex() const;

    ControlSlotType& GetSlot();

    void Reset();

  private:
    SlotIndexType slot_index_;
    ControlSlotType* slot_pointer_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SLOT_INDICATOR_H
