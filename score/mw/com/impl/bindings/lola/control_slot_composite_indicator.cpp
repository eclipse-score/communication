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
#include "score/mw/com/impl/bindings/lola/control_slot_composite_indicator.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::lola
{

// Suppress "AUTOSAR C++14 A12-1-5" rule finding. This rule declares: "Common class initialization for non-constant
// members shall be done by a delegating constructor.".
// Here we can not delegate to the 2nd ctor as it needs VALID references forControlSlotType args!
// coverity[autosar_cpp14_a12_1_5_violation]
ControlSlotCompositeIndicator::ControlSlotCompositeIndicator() noexcept
    : slot_index_{0U}, slot_pointer_qm_{nullptr}, slot_pointer_asil_b_{nullptr}
{
}

ControlSlotCompositeIndicator::ControlSlotCompositeIndicator(SlotIndexType slot_index,
                                                             ControlSlotType& slot_qm,
                                                             ControlSlotType& slot_asil_b)
    : slot_index_{slot_index}, slot_pointer_qm_{&slot_qm}, slot_pointer_asil_b_{&slot_asil_b}
{
}

ControlSlotCompositeIndicator::ControlSlotCompositeIndicator(SlotIndexType slot_index,
                                                             ControlSlotType& slot,
                                                             CompositeSlotTagType type)
    : slot_index_{slot_index},
      slot_pointer_qm_{type == CompositeSlotTagType::QM ? &slot : nullptr},
      slot_pointer_asil_b_{type == CompositeSlotTagType::ASIL_B ? &slot : nullptr}
{
}

bool ControlSlotCompositeIndicator::IsValidQmAndAsilB() const
{
    return ((slot_pointer_qm_ != nullptr) && (slot_pointer_asil_b_ != nullptr));
}

bool ControlSlotCompositeIndicator::IsValidQM() const
{
    return (slot_pointer_qm_ != nullptr);
}

bool ControlSlotCompositeIndicator::IsValidAsilB() const
{
    return (slot_pointer_asil_b_ != nullptr);
}

SlotIndexType ControlSlotCompositeIndicator::GetIndex() const
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD((slot_pointer_qm_ != nullptr) || (slot_pointer_asil_b_ != nullptr));
    return slot_index_;
}

ControlSlotType& ControlSlotCompositeIndicator::GetSlotQM()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_pointer_qm_ != nullptr);
    return *slot_pointer_qm_;
}

const ControlSlotType& ControlSlotCompositeIndicator::GetSlotQM() const
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_pointer_qm_ != nullptr);
    return *slot_pointer_qm_;
}

ControlSlotType& ControlSlotCompositeIndicator::GetSlotAsilB()
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_pointer_asil_b_ != nullptr);
    return *slot_pointer_asil_b_;
}

const ControlSlotType& ControlSlotCompositeIndicator::GetSlotAsilB() const
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(slot_pointer_asil_b_ != nullptr);
    return *slot_pointer_asil_b_;
}

void ControlSlotCompositeIndicator::Reset()
{
    slot_pointer_qm_ = nullptr;
    slot_pointer_asil_b_ = nullptr;
}

bool operator==(const ControlSlotCompositeIndicator& lhs, const ControlSlotCompositeIndicator& rhs) noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states:"The operands of a logical && or \\ shall be
    // parenthesized if the operands contain binary operators".
    // This a false-positive, all operands are parenthesized.
    // A bug ticket has been created to track this: [Ticket-165315](broken_link_j/Ticket-165315)
    // coverity[autosar_cpp14_a5_2_6_violation : FALSE]
    return (lhs.IsValidQM() == rhs.IsValidQM()) && (lhs.IsValidAsilB() == rhs.IsValidAsilB()) &&
           ((lhs.IsValidQM() == false) || (lhs.GetIndex() == rhs.GetIndex())) &&
           ((lhs.IsValidQM() == false) || (&(lhs.GetSlotQM()) == &(rhs.GetSlotQM()))) &&
           ((lhs.IsValidAsilB() == false) || (&(lhs.GetSlotAsilB()) == &(rhs.GetSlotAsilB())));
}

}  // namespace score::mw::com::impl::lola
