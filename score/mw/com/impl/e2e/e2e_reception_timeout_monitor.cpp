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
#include "score/mw/com/impl/e2e/e2e_reception_timeout_monitor.h"

namespace score::mw::com::impl::e2e
{

ReceptionTimeoutMonitor::ReceptionTimeoutMonitor(const std::chrono::milliseconds reception_timeout) noexcept
    : reception_timeout_{reception_timeout}, reference_{}, is_armed_{false}
{
}

bool ReceptionTimeoutMonitor::IsEnabled() const noexcept
{
    return reception_timeout_ > Clock::duration::zero();
}

void ReceptionTimeoutMonitor::OnActivity(const Clock::time_point now) noexcept
{
    reference_ = now;
    is_armed_ = true;
}

std::uint8_t ReceptionTimeoutMonitor::ConsumeMissedPeriods(const Clock::time_point now) noexcept
{
    if ((!IsEnabled()) || (!is_armed_) || (now < reference_))
    {
        return 0U;
    }

    const auto periods = (now - reference_) / reception_timeout_;
    // Advance by whole periods only, so a partly elapsed period is counted on a later call.
    reference_ += periods * reception_timeout_;
    return (periods >= kMaxHealthWindowSize) ? kMaxHealthWindowSize : static_cast<std::uint8_t>(periods);
}

void ReceptionTimeoutMonitor::Reset() noexcept
{
    is_armed_ = false;
}

}  // namespace score::mw::com::impl::e2e
