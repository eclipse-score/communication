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
    std::uint8_t window_size;
};

// State kept per consumer.
struct HealthContext
{
    // Bit 0 is the newest sample; a set bit marks a failed sample.
    std::uint64_t failed_samples{0U};

    // Number of valid samples in the window (<= window_size).
    std::uint8_t samples_in_window{0U};

    // false = kOk, true = kError.
    bool is_currently_error{false};
};

// Terminates if the configuration is inconsistent. Call once at construction.
void ValidateHealthTrackerConfiguration(const HealthTrackerConfiguration& config) noexcept;

// Updates the state with one checked sample and returns the resulting status.
// The window is cleared when the state flips.
HistoricalHealthStatus UpdateHistoricalHealth(DataIntegrityStatus data_integrity,
                                              SequenceStatus sequence,
                                              const HealthTrackerConfiguration& config,
                                              HealthContext& context) noexcept;

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_H
