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
#ifndef SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_H
#define SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_H

#include "score/mw/com/impl/e2e/e2e_health_tracker_configuration.h"
#include "score/mw/com/impl/e2e/e2e_result.h"

#include <cstdint>
#include <optional>

namespace score::mw::com::impl::e2e
{

// Tracks the historical health of one consumer: whether its E2E-checked samples have recently been failing or
// passing, as opposed to the pass/fail status of a single sample.
//
// A single failed (or recovered) sample does not flip the status by itself: the channel only becomes unhealthy
// (kError) once error_threshold of the last window_size samples failed, and only becomes healthy again (kOk) once
// recovery_threshold subsequent samples passed (see HealthTrackerConfiguration). This avoids flipping to unhealthy on
// a transient glitch, and avoids flipping back to healthy on a single lucky sample while mostly still failing.
//
// Each instance owns its own sample window; samples only leave the window by aging out.
class HealthTracker
{
  public:
    // An empty config means historical health tracking is disabled; Update() then always returns
    // HistoricalHealthStatus::kDisabled.
    explicit HealthTracker(const std::optional<HealthTrackerConfiguration>& config) noexcept;

    // Records one checked sample and returns the resulting status (kDisabled if the tracker is disabled).
    HistoricalHealthStatus Update(DataIntegrityStatus data_integrity, SequenceStatus sequence) noexcept;

    // Clears the sample window and returns to kOk. To be called when communication continuity is broken (e.g.
    // StopOfferEvent -> ReOfferEvent or resubscribe), so that old samples do not cause kError after a restart.
    void Reset() noexcept;

  private:
    std::optional<HealthTrackerConfiguration> config_;

    // Bit 0 is the newest sample; a set bit marks a failed sample.
    std::uint64_t failed_samples_;

    // Number of valid samples in the window (<= window_size).
    std::uint32_t samples_in_window_;

    // false = kOk, true = kError.
    bool is_currently_error_;
};

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_H
