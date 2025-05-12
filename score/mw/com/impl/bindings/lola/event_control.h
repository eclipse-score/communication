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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_CONTROL_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_CONTROL_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"

namespace score::mw::com::impl::lola
{

class EventControl
{
  public:
    using SubscriberCountType = EventSubscriptionControl::SubscriberCountType;
    EventControl(const SlotIndexType number_of_slots,
                 const SubscriberCountType max_subscribers,
                 const bool enforce_max_samples,
                 const score::memory::shared::MemoryResourceProxy* const proxy) noexcept;

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventDataControl data_control;
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventSubscriptionControl subscription_control;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_CONTROL_H
