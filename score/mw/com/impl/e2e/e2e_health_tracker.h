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

#include <chrono>
#include <cstdint>

namespace score::mw::com::impl::e2e
{

// Largest supported window size (one bit per sample).
constexpr std::uint8_t kMaxHealthWindowSize{64U};

// Looks at the last window_size slots. A slot is a passed sample, a failed sample or a period without new data.
// In kOk, error_threshold failed samples switch to kError. So does a full window with fewer than min_ok_threshold
// passed samples. In kError, recovery_threshold passed samples switch back to kOk.
// Periods without new data are neutral: they never count as failures.
// The thresholds and the window size are only validated and used when enabled is true.
struct HealthTrackerConfiguration
{
    bool enabled{false};

    // Failed samples in the window needed to switch to kError. Must be >= 1.
    std::uint8_t error_threshold{1U};

    // Passed samples in the window needed to switch back to kOk. Must be >= 1.
    std::uint8_t recovery_threshold{1U};

    // Number of most recent samples considered. Must be >= both thresholds and <= kMaxHealthWindowSize.
    // Must also be < error_threshold + recovery_threshold, otherwise the state could oscillate between kOk and kError.
    std::uint8_t window_size{1U};

    // Passed samples a full window needs to stay kOk. 0 disables the rule.
    // Must be <= window_size - error_threshold + 1, so failures alone still switch exactly at error_threshold. Together
    // with error_threshold + recovery_threshold > window_size this also keeps it <= recovery_threshold.
    std::uint8_t min_ok_threshold{0U};

    // Expected maximum time between two samples. Each elapsed period without a sample is one neutral slot.
    // 0 disables reception supervision. Requires min_ok_threshold > 0, otherwise missing data has no effect.
    std::chrono::milliseconds reception_timeout{0};
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

    // Records count periods without new data as neutral slots and returns the resulting status.
    HistoricalHealthStatus RecordNoNewData(std::uint8_t count) noexcept;

    // Returns the current status without recording anything.
    HistoricalHealthStatus GetStatus() const noexcept;

    // Clears the window and returns to kOk. Called on user (re)subscribe.
    void Reset() noexcept;

  private:
    void PushSlot(bool passed, bool failed) noexcept;
    void UpdateState() noexcept;

    HealthTrackerConfiguration config_;

    // Bit 0 is the newest slot; a set bit marks a failed sample.
    std::uint64_t failed_samples_;

    // Bit 0 is the newest slot; a set bit marks a passed sample. Slots set in neither mask carry no new data.
    std::uint64_t passed_samples_;

    // Number of valid slots in the window (<= window_size).
    std::uint8_t samples_in_window_;

    // false = kOk, true = kError.
    bool is_currently_error_;
};

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_H
