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
#ifndef SCORE_AAS_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_CONTROL_LOCAL_VIEW_H
#define SCORE_AAS_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_CONTROL_LOCAL_VIEW_H

#include "score/mw/com/impl/bindings/lola/event_control.h"
#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"
#include "score/mw/com/impl/bindings/lola/proxy_event_data_control_local_view.h"

#include <functional>

namespace score::mw::com::impl::lola
{

class ProxyEventControlLocalView
{
  public:
    using SubscriberCountType = EventSubscriptionControl::SubscriberCountType;
    ProxyEventControlLocalView(EventControl& event_control_shared_mem) noexcept;

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    ProxyEventDataControlLocalView<> data_control;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::reference_wrapper<EventSubscriptionControl> subscription_control;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_AAS_MW_COM_IMPL_BINDINGS_LOLA_PROXY_EVENT_CONTROL_LOCAL_VIEW_H
