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
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed_composite.h"

namespace score::mw::com::impl::lola
{

EventDataControlMixedComposite::EventDataControlMixedComposite(EventDataControlMixed& qm_control) noexcept
    : composite_{qm_control.data_control_}, qm_control_{qm_control}, asil_b_control_{nullptr}
{
}

EventDataControlMixedComposite::EventDataControlMixedComposite(EventDataControlMixed& qm_control,
                                                               EventDataControlMixed* const asil_b_control) noexcept
    : composite_{qm_control.data_control_, (asil_b_control != nullptr) ? &asil_b_control->data_control_ : nullptr},
      qm_control_{qm_control},
      asil_b_control_{asil_b_control}
{
}

void EventDataControlMixedComposite::EventReady(const SlotIndexType slot_index,
                                                EventSlotStatus::EventTimeStamp time_stamp) noexcept
{
    composite_.EventReady(slot_index, time_stamp);
    qm_control_.get().newest_index_ = slot_index;
    if (asil_b_control_ != nullptr)
    {
        asil_b_control_->newest_index_ = slot_index;
    }
}

EventDataControlComposite<>& EventDataControlMixedComposite::GetComposite() noexcept
{
    return composite_;
}

}  // namespace score::mw::com::impl::lola
