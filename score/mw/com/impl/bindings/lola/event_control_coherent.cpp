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
#include "score/mw/com/impl/bindings/lola/event_control_coherent.h"

namespace score::mw::com::impl::lola
{

EventControlCoherent::EventControlCoherent(const SlotIndexType number_of_slots,
                                           const SubscriberCountType max_subscribers,
                                           const bool enforce_max_samples,
                                           score::memory::shared::ManagedMemoryResource& resource)
    : data_control{number_of_slots, resource},
      subscription_control{number_of_slots, max_subscribers, enforce_max_samples},
      // CoherencyType::kCoherent events need no per-slot transaction logging (0 slots), but still need
      // Subscribe/Unsubscribe transaction logging for crash-recovery of the subscriber count.
      transaction_log_set_{max_subscribers, 0U, resource}
{
}

}  // namespace score::mw::com::impl::lola
