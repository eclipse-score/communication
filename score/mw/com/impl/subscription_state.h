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
#ifndef SCORE_MW_COM_SUBSCRIPTIONSTATE_H
#define SCORE_MW_COM_SUBSCRIPTIONSTATE_H

#include <cstdint>

namespace score::mw::com::impl
{

/// \api
/// \brief state of a proxy event.
/// \requirement SWS_CM_00310
/// \public
enum class SubscriptionState : std::uint8_t
{
    kSubscribed,           ///< Proxy is subscribed to a particular event and will receive data.
    kNotSubscribed,        ///< Proxy is not subscribed, no data is received.
    kSubscriptionPending,  ///< Subscription is requested but not yet acknowledged by the producer.
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_SUBSCRIPTIONSTATE_H
