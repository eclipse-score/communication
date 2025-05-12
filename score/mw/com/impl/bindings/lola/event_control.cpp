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
#include "score/mw/com/impl/bindings/lola/event_control.h"

namespace score::mw::com::impl::lola
{

EventControl::EventControl(const SlotIndexType number_of_slots,
                           const SubscriberCountType max_subscribers,
                           const bool enforce_max_samples,
                           const score::memory::shared::MemoryResourceProxy* const proxy) noexcept
    : data_control{number_of_slots, proxy, max_subscribers},
      subscription_control{number_of_slots, max_subscribers, enforce_max_samples}
{
}

}  // namespace score::mw::com::impl::lola
