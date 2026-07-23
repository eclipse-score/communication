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
enum class SubscriptionState : std::uint8_t
{
    /// kSubscribed is entered when the subscription has been successfully acknowledged by the provider side and the
    /// consumer has not been notified that the service is no longer offered (either the provider has called
    /// stop_offer() or crashed).
    kSubscribed,

    /// kNotSubscribed is entered when Subscribe() has not been called or the last successful Subscribe() has been
    /// withdrawn with call to Unsubscribe() Last call to Subscribe() has been rejected/negatively acknowledged by the
    /// provider side or provider side explicitly cancels an already acknowledged subscription.
    kNotSubscribed,

    /// kSubscriptionPending state is entered when the Subscription() call is either in the state to being dispatched to
    /// the provider side or has been already dispatched to the provider side, but acknowlegde from provider side is
    /// pending state was already in kSubscribed, but then the consumer has been notified that the whole enclosing
    /// providing service instance has stopped offering. This is the "auto-reconnect mode". As soon as the consumer is
    /// notified that the service instance is being offered again, the state would transition back to kSubscribed.
    kSubscriptionPending,
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_SUBSCRIPTIONSTATE_H
