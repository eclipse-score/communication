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
#ifndef SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_CONFIGURATION_H
#define SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_CONFIGURATION_H

#include <cstdint>

namespace score::mw::com::impl::e2e
{

// Largest supported window size (one bit per sample).
constexpr std::uint32_t kMaxHealthWindowSize{64U};

// Looks at the last window_size samples. In kOk, error_threshold failed samples switch to kError.
// In kError, recovery_threshold passed samples switch back to kOk.
//
// Always describes an enabled configuration: whether historical health tracking is enabled at all is not part of
// this class, see HealthTracker, which is constructed from a std::optional<HealthTrackerConfiguration> (an empty
// optional means tracking is disabled). The constructor validates its arguments and terminates immediately if they
// are inconsistent, instead of deferring that validation to HealthTracker's constructor.
class HealthTrackerConfiguration
{
  public:
    // Terminates if the combination is inconsistent:
    // - error_threshold and recovery_threshold must each be >= 1
    // - window_size must be >= both thresholds and <= kMaxHealthWindowSize
    // - error_threshold + recovery_threshold must exceed window_size, otherwise the state could oscillate between
    //   kOk and kError
    HealthTrackerConfiguration(std::uint8_t error_threshold, std::uint8_t recovery_threshold, std::uint8_t window_size);

    constexpr std::uint8_t GetErrorThreshold() const
    {
        return error_threshold_;
    }

    constexpr std::uint8_t GetRecoveryThreshold() const
    {
        return recovery_threshold_;
    }

    constexpr std::uint8_t GetWindowSize() const
    {
        return window_size_;
    }

  private:
    std::uint8_t error_threshold_;
    std::uint8_t recovery_threshold_;
    std::uint8_t window_size_;
};

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_CONFIGURATION_H
