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
#include "score/mw/com/impl/bindings/lola/slot_decrementer.h"

namespace score::mw::com::impl::lola
{

SlotDecrementer::SlotDecrementer(EventDataControl* event_data_control,
                                 ControlSlotIndicator control_slot_indicator,
                                 const TransactionLogSet::TransactionLogIndex transaction_log_idx) noexcept
    : event_data_control_{event_data_control},
      control_slot_indicator_{control_slot_indicator},
      transaction_log_idx_{transaction_log_idx}
{
}

SlotDecrementer::~SlotDecrementer() noexcept
{
    internal_delete();
}

SlotDecrementer::SlotDecrementer(SlotDecrementer&& other) noexcept
    : event_data_control_{other.event_data_control_},
      control_slot_indicator_{std::move(other.control_slot_indicator_)},
      transaction_log_idx_{other.transaction_log_idx_}
{
    other.event_data_control_ = nullptr;
}

// Suppress "AUTOSAR C++14 A6-2-1" rule violation. The rule states "Move and copy assignment operators shall either move
// or respectively copy base classes and data members of a class, without any side effects." Due to architectural
// decisions, SlotDecrementer must perform a cleanup and update references to itself in its events and fields.
// Therefore, side effects are required.
// coverity[autosar_cpp14_a6_2_1_violation]
SlotDecrementer& SlotDecrementer::operator=(SlotDecrementer&& other) noexcept
{
    if (this != &other)
    {
        internal_delete();
        event_data_control_ = other.event_data_control_;
        control_slot_indicator_ = other.control_slot_indicator_;
        transaction_log_idx_ = other.transaction_log_idx_;

        other.event_data_control_ = nullptr;
    }
    return *this;
}

void SlotDecrementer::internal_delete() noexcept
{
    if (event_data_control_ != nullptr)
    {
        event_data_control_->DereferenceEvent(control_slot_indicator_, transaction_log_idx_);
    }
}

}  // namespace score::mw::com::impl::lola
