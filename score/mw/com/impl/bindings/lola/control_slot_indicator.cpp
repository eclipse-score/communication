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
#include "score/mw/com/impl/bindings/lola/control_slot_indicator.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::lola
{
// Suppress "AUTOSAR C++14 A12-1-5" rule finding. This rule declares: "Common class initialization for non-constant
// members shall be done by a delegating constructor.".
// Here we can not delegate to the 2nd ctor as it needs VALID reference for ControlSlotType!
// coverity[autosar_cpp14_a12_1_5_violation]
ControlSlotIndicator::ControlSlotIndicator() : slot_index_{0U}, slot_pointer_{nullptr} {}

ControlSlotIndicator::ControlSlotIndicator(SlotIndexType slot_index, ControlSlotType& slot)
    : slot_index_{slot_index}, slot_pointer_{&slot}
{
}

bool ControlSlotIndicator::IsValid() const
{
    return (slot_pointer_ != nullptr);
}

SlotIndexType ControlSlotIndicator::GetIndex() const
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_pointer_ != nullptr);
    return slot_index_;
}

ControlSlotType& ControlSlotIndicator::GetSlot()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_pointer_ != nullptr);
    return *slot_pointer_;
}

void ControlSlotIndicator::Reset()
{
    slot_pointer_ = nullptr;
}

}  // namespace score::mw::com::impl::lola
