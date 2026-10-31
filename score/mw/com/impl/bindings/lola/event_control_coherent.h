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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_CONTROL_COHERENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_CONTROL_COHERENT_H

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_coherent.h"
#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"

namespace score::mw::com::impl::lola
{

/// \brief EventControlCoherent is the CoherencyType::kCoherent counterpart of EventControl.
///
/// \details It differs from EventControl in two ways:
/// - data_control is of type EventDataControlCoherent instead of EventDataControl.
/// - its transaction_log_set_ is constructed with 0 slots: in CoherencyType::kCoherent mode, consumers never alter
///   any per-slot state in the CONTROL segment (no reference-counting, see CoherentEventSlotStatus), so no
///   transaction-log based crash-recovery is required for slot increment/decrement operations. Subscribe/Unsubscribe
///   transaction logging (crash-recovery of the subscriber count), however, is still required and provided by
///   transaction_log_set_.
class EventControlCoherent
{
  public:
    using SubscriberCountType = EventSubscriptionControl<>::SubscriberCountType;
    EventControlCoherent(const SlotIndexType number_of_slots,
                         const SubscriberCountType max_subscribers,
                         const bool enforce_max_samples,
                         score::memory::shared::ManagedMemoryResource& resource);

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    EventDataControlCoherent data_control;

    // coverity[autosar_cpp14_m11_0_1_violation]
    EventSubscriptionControl<> subscription_control;

    // coverity[autosar_cpp14_m11_0_1_violation]
    TransactionLogSet transaction_log_set_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_EVENT_CONTROL_COHERENT_H
