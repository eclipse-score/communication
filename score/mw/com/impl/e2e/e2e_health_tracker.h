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

/// \brief Configuration for the binding-independent historical health tracker.
///
/// One configuration is shared by all samples of a given event/field; it is resolved once from the
/// deployment configuration and does not change afterwards.
struct HealthTrackerConfiguration
{
    /// \brief Whether historical health tracking is active for this event/field.
    bool enabled;

    /// \brief Number of accumulated errors at/above which the tracker latches into HistoricalHealthStatus::kError.
    /// Must be >= 1.
    std::uint8_t error_threshold;

    /// \brief Number of accumulated errors at/below which the tracker latches back into
    /// HistoricalHealthStatus::kOk. Must be strictly less than error_threshold.
    std::uint8_t recovery_threshold;
};

/// \brief Mutable, per-consumer state of the historical health tracker.
///
/// Cardinality mirrors ProtectContext/CheckContext: one HealthContext instance per consuming proxy event.
struct HealthContext
{
    /// \brief Saturating error counter: incremented on a failed sample, decremented on a passed one.
    std::uint8_t error_counter{0U};

    /// \brief Current latched state of the hysteresis (true once error_threshold is reached, false again
    /// only once recovery_threshold is reached).
    bool is_currently_error{false};
};

/// \brief Validates that a HealthTrackerConfiguration is internally consistent.
///
/// Intended to be called once, when the configuration is resolved (e.g. at construction/deployment-resolution
/// time), not per message. Terminates the process on violation, mirroring the precondition-check idiom used
/// elsewhere in this codebase (see EnrichedInstanceIdentifier).
void ValidateHealthTrackerConfiguration(const HealthTrackerConfiguration& config) noexcept;

/// \brief Updates the historical health hysteresis from one already-checked sample's categorical results
/// and returns the resulting HistoricalHealthStatus.
///
/// Binding-independent: only consumes the abstracted DataIntegrityStatus/SequenceStatus of the sample,
/// never binding-specific wire data. Must only be called once per received/checked sample.
///
/// Validates config on every call (see ValidateHealthTrackerConfiguration) so that an inconsistent
/// configuration can never silently latch the tracker into a wrong state.
HistoricalHealthStatus UpdateHistoricalHealth(DataIntegrityStatus data_integrity,
                                              SequenceStatus sequence,
                                              const HealthTrackerConfiguration& config,
                                              HealthContext& context) noexcept;

}  // namespace score::mw::com::impl::e2e

#endif  // SCORE_MW_COM_IMPL_E2E_E2E_HEALTH_TRACKER_H
