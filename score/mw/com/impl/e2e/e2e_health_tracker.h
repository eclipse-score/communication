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
constexpr std::uint32_t kMaxHealthWindowSize{64U};

// Looks at the last window_size samples. In kOk, error_threshold failed samples switch to kError.
// In kError, recovery_threshold passed samples switch back to kOk.
// The thresholds and the window size are only read and validated when enabled.
//
// A default-constructed (or CreateDisabled()) instance is always well-formed. CreateEnabled() validates its
// arguments and terminates immediately if they are inconsistent, instead of deferring that validation to
// HealthTracker's constructor.
class HealthTrackerConfiguration
{

  public:
    // Historical health tracking is off; Update() always returns HistoricalHealthStatus::kDisabled.
    static HealthTrackerConfiguration CreateDisabled() noexcept;

    // Terminates if the combination is inconsistent:
    // - error_threshold and recovery_threshold must each be >= 1
    // - window_size must be >= both thresholds and <= kMaxHealthWindowSize
    // - error_threshold + recovery_threshold must exceed window_size, otherwise the state could oscillate between
    //   kOk and kError
    static HealthTrackerConfiguration CreateEnabled(std::uint8_t error_threshold,
                                                    std::uint8_t recovery_threshold,
                                                    std::uint8_t window_size) noexcept;

    // Equivalent to CreateDisabled().
    constexpr HealthTrackerConfiguration() noexcept : HealthTrackerConfiguration{false, 1U, 1U, 1U} {}

    constexpr bool IsEnabled() const noexcept
    {
        return enabled_;
    }

    constexpr std::uint8_t GetErrorThreshold() const noexcept
    {
        return error_threshold_;
    }

    constexpr std::uint8_t GetRecoveryThreshold() const noexcept
    {
        return recovery_threshold_;
    }

    constexpr std::uint8_t GetWindowSize() const noexcept
    {
        return window_size_;
    }

  private:
    constexpr HealthTrackerConfiguration(const bool enabled,
                                         const std::uint8_t error_threshold,
                                         const std::uint8_t recovery_threshold,
                                         const std::uint8_t window_size) noexcept
        : enabled_{enabled},
          error_threshold_{error_threshold},
          recovery_threshold_{recovery_threshold},
          window_size_{window_size}
    {
    }

    bool enabled_;
    std::uint8_t error_threshold_;
    std::uint8_t recovery_threshold_;
    std::uint8_t window_size_;
};

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
    explicit HealthTracker(const HealthTrackerConfiguration& config) noexcept;

    // Records one checked sample and returns the resulting status (kDisabled if the tracker is disabled).
    HistoricalHealthStatus Update(DataIntegrityStatus data_integrity, SequenceStatus sequence) noexcept;

    // Clears the sample window and returns to kOk. To be called when communication continuity is broken (e.g.
    // StopOfferEvent -> ReOfferEvent or resubscribe), so that old samples do not cause kError after a restart.
    void Reset() noexcept;

  private:
    HealthTrackerConfiguration config_;

    // Bit 0 is the newest sample; a set bit marks a failed sample.
    std::uint64_t failed_samples_;

    // Number of valid samples in the window (<= window_size).
    std::uint32_t samples_in_window_;

    // false = kOk, true = kError.
    bool is_currently_error_;
};

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_H
