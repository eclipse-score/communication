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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SLOT_COMPOSITE_INDICATOR_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SLOT_COMPOSITE_INDICATOR_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"

#include <atomic>
#include <cstddef>

namespace score::mw::com::impl::lola
{

/// \brief Helper class (similar to ControlSlotIndicator), which identifies a slot in our "control slot array" using
///        both the slot index and a raw pointer to the element.
/// \details Opposed to ControlSlotIndicator, this is an slot indicator for QM slots AND ASIL-B slots! Normally such a
///          ControlSlotCompositeIndicator returned e.g. by a slot allocation for an event/field supporting QM and
///          ASIL-B contains a slot-pointer for a QM slot and an ASIL-B slot. If the underlying event/field just has
///          QM support only a valid slot-pointer for QM is contained. In case only a ASIL-B slot pointer is contained
///          (#IsValidAsilB() = true), but no QM slot pointer is contained (#IsValidQM() = false), then we have the
///          case, where QM consumers have been disconnected and the composite is falling back to ASIL-B only.
class ControlSlotCompositeIndicator
{
  public:
    enum CompositeSlotTagType
    {
        QM,
        ASIL_B
    };
    ControlSlotCompositeIndicator() noexcept;

    ControlSlotCompositeIndicator(SlotIndexType slot_index, ControlSlotType& slot_qm, ControlSlotType& slot_asil_b);

    ControlSlotCompositeIndicator(SlotIndexType slot_index, ControlSlotType& slot, CompositeSlotTagType type);

    bool IsValidQmAndAsilB() const;

    bool IsValidQM() const;

    bool IsValidAsilB() const;

    SlotIndexType GetIndex() const;

    ControlSlotType& GetSlotQM();

    const ControlSlotType& GetSlotQM() const;

    ControlSlotType& GetSlotAsilB();

    const ControlSlotType& GetSlotAsilB() const;

    void Reset();

  private:
    SlotIndexType slot_index_;
    ControlSlotType* slot_pointer_qm_;
    ControlSlotType* slot_pointer_asil_b_;
};

bool operator==(const ControlSlotCompositeIndicator& lhs, const ControlSlotCompositeIndicator& rhs) noexcept;

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SLOT_COMPOSITE_INDICATOR_H
