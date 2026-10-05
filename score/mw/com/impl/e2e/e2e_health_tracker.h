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

/// \brief Configuration of the historical health tracker (shared per event/field).
struct HealthTrackerConfiguration
{
    /// \brief Whether tracking is active.
    bool enabled;

    /// \brief Error count at/above which the state latches to kError. Must be >= 1.
    std::uint8_t error_threshold;

    /// \brief Error count at/below which the state latches back to kOk. Must be < error_threshold.
    std::uint8_t recovery_threshold;
};

/// \brief Per-consumer state of the historical health tracker.
struct HealthContext
{
    /// \brief Saturating counter: +1 on a failed sample, -1 on a passed one.
    std::uint8_t error_counter{0U};

    /// \brief Latched hysteresis state.
    bool is_currently_error{false};
};

/// \brief Terminates if the configuration is inconsistent.
void ValidateHealthTrackerConfiguration(const HealthTrackerConfiguration& config) noexcept;

/// \brief Updates the hysteresis from one checked sample and returns the resulting status.
///
/// Call once per sample. Validates the configuration on every call.
HistoricalHealthStatus UpdateHistoricalHealth(DataIntegrityStatus data_integrity,
                                              SequenceStatus sequence,
                                              const HealthTrackerConfiguration& config,
                                              HealthContext& context) noexcept;

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_H
