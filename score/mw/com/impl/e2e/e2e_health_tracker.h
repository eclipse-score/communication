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

#include "score/mw/com/impl/e2e/e2e_result.h"

#include <cstdint>

namespace score::mw::com::impl::e2e
{

// Largest supported window size (one bit per sample).
constexpr std::uint8_t kMaxHealthWindowSize{64U};

// Looks at the last window_size samples. In kOk, error_threshold failed samples switch to kError.
// In kError, recovery_threshold passed samples switch back to kOk.
struct HealthTrackerConfiguration
{
    bool enabled;

    // Failed samples in the window needed to switch to kError. Must be >= 1.
    std::uint8_t error_threshold;

    // Passed samples in the window needed to switch back to kOk. Must be >= 1.
    std::uint8_t recovery_threshold;

    // Number of most recent samples considered. Must be >= both thresholds and <= kMaxHealthWindowSize.
    // Must also be < error_threshold + recovery_threshold, otherwise the state could oscillate between kOk and kError.
    std::uint8_t window_size;
};

// Tracks the historical health of one consumer. Each instance owns its own sample window.
// The sample history is kept when the state flips; samples only leave the window by aging out.
class HealthTracker
{
  public:
    // Terminates if the configuration is inconsistent.
    explicit HealthTracker(const HealthTrackerConfiguration& config) noexcept;

    // Records one checked sample and returns the resulting status (kDisabled if the tracker is disabled).
    HistoricalHealthStatus Update(DataIntegrityStatus data_integrity, SequenceStatus sequence) noexcept;

  private:
    HealthTrackerConfiguration config_;

    // Bit 0 is the newest sample; a set bit marks a failed sample.
    std::uint64_t failed_samples_;

    // Number of valid samples in the window (<= window_size).
    std::uint8_t samples_in_window_;

    // false = kOk, true = kError.
    bool is_currently_error_;
};

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_H
