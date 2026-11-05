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
#ifndef SCORE_MW_COM_IMPL_E2E_E2E_RECEPTION_TIMEOUT_MONITOR_H
#define SCORE_MW_COM_IMPL_E2E_E2E_RECEPTION_TIMEOUT_MONITOR_H

#include "score/mw/com/impl/e2e/e2e_health_tracker.h"

#include <chrono>
#include <cstdint>

namespace score::mw::com::impl::e2e
{

// Counts reception timeout periods that elapsed without new data. Never reads a clock: the caller passes the
// current time, so timing stays under the control of the application.
class ReceptionTimeoutMonitor
{
  public:
    using Clock = std::chrono::steady_clock;

    // A reception_timeout of 0 disables the monitor.
    explicit ReceptionTimeoutMonitor(std::chrono::milliseconds reception_timeout = {}) noexcept;

    bool IsEnabled() const noexcept;

    // Marks new data at now. The first call after construction or Reset() arms the monitor.
    void OnActivity(Clock::time_point now) noexcept;

    // Returns the number of whole periods elapsed without activity that were not returned before, capped at
    // kMaxHealthWindowSize. Returns 0 while disabled or not armed.
    std::uint8_t ConsumeMissedPeriods(Clock::time_point now) noexcept;

    // Disarms the monitor until the next OnActivity().
    void Reset() noexcept;

  private:
    Clock::duration reception_timeout_;

    // End of the last period already accounted for (last activity or last consumed period).
    Clock::time_point reference_;

    bool is_armed_;
};

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_RECEPTION_TIMEOUT_MONITOR_H
