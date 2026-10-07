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
#include "score/mw/com/impl/e2e/e2e_summary.h"

namespace score::mw::com::impl::e2e
{

Summary ComputeSummary(const DataIntegrityStatus data_integrity,
                       const SequenceStatus sequence,
                       const HistoricalHealthStatus historical_health) noexcept
{
    const bool data_integrity_failed{data_integrity == DataIntegrityStatus::kError};
    const bool sequence_failed{(sequence == SequenceStatus::kErrorRepeated) ||
                               (sequence == SequenceStatus::kErrorGapExceedsThreshold)};
    const bool historical_health_failed{historical_health == HistoricalHealthStatus::kError};

    if (data_integrity_failed || sequence_failed || historical_health_failed)
    {
        return Summary::kError;
    }

    const bool data_integrity_disabled{data_integrity == DataIntegrityStatus::kDisabled};
    const bool sequence_disabled{sequence == SequenceStatus::kDisabled};
    const bool historical_health_disabled{historical_health == HistoricalHealthStatus::kDisabled};

    if (data_integrity_disabled && sequence_disabled && historical_health_disabled)
    {
        return Summary::kDisabled;
    }

    if (data_integrity_disabled || sequence_disabled || historical_health_disabled)
    {
        return Summary::kOkWithDisabledChecks;
    }

    return Summary::kOk;
}

E2EResult BuildE2EResult(const DataIntegrityStatus data_integrity,
                         const SequenceStatus sequence,
                         HealthTracker& health_tracker) noexcept
{
    const HistoricalHealthStatus historical_health{health_tracker.Update(data_integrity, sequence)};
    const Summary summary{ComputeSummary(data_integrity, sequence, historical_health)};

    return E2EResult{data_integrity, sequence, historical_health, summary};
}

}  // namespace score::mw::com::impl::e2e
