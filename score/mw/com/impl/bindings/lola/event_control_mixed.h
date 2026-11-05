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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_CONTROL_MIXED_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_CONTROL_MIXED_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed.h"
#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

namespace score::mw::com::impl::lola
{

/// \brief EventControlMixed is the CoherencyType::kMixed counterpart of EventControl / EventControlCoherent.
///
/// \details It differs from EventControl only in that data_control is of type EventDataControlMixed instead of
/// EventDataControl. Its transaction_log_set_ is, unlike EventControlCoherent's, constructed with the full
/// number_of_slots (same as EventControl's): non-coherent consumers of a CoherencyType::kMixed event alter per-slot
/// state in the CONTROL segment exactly as for CoherencyType::kNonCoherent, and therefore still require per-slot
/// transaction-log based crash-recovery.
class EventControlMixed
{
  public:
    using SubscriberCountType = EventSubscriptionControl<>::SubscriberCountType;
    EventControlMixed(const SlotIndexType number_of_slots,
                      const SubscriberCountType max_subscribers,
                      const bool enforce_max_samples,
                      score::memory::shared::ManagedMemoryResource& resource);

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventDataControlMixed data_control;

    // coverity[autosar_cpp14_m11_0_1_violation]
    EventSubscriptionControl<> subscription_control;

    // coverity[autosar_cpp14_m11_0_1_violation]
    TransactionLogSet transaction_log_set_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_CONTROL_MIXED_H
