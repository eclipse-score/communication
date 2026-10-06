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
#include "score/mw/com/impl/e2e/e2e_health_tracker.h"

#include <score/assert.hpp>

#include <limits>

namespace score::mw::com::impl::e2e
{

void ValidateHealthTrackerConfiguration(const HealthTrackerConfiguration& config) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config.error_threshold >= 1U,
                                                      "error_threshold must be at least 1");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config.recovery_threshold < config.error_threshold,
                                                      "recovery_threshold must be strictly less than error_threshold");
}

HistoricalHealthStatus UpdateHistoricalHealth(const DataIntegrityStatus data_integrity,
                                              const SequenceStatus sequence,
                                              const HealthTrackerConfiguration& config,
                                              HealthContext& context) noexcept
{
    if (!config.enabled)
    {
        return HistoricalHealthStatus::kDisabled;
    }

    const bool data_integrity_failed{data_integrity == DataIntegrityStatus::kError};
    const bool sequence_failed{(sequence == SequenceStatus::kErrorRepeated) ||
                               (sequence == SequenceStatus::kErrorGapExceedsThreshold)};
    const bool this_sample_failed{data_integrity_failed || sequence_failed};

    if (this_sample_failed)
    {
        if (context.error_counter < std::numeric_limits<std::uint8_t>::max())
        {
            ++context.error_counter;
        }
    }
    else if (context.error_counter > 0U)
    {
        --context.error_counter;
    }

    if (context.error_counter >= config.error_threshold)
    {
        context.is_currently_error = true;
    }
    else if (context.error_counter <= config.recovery_threshold)
    {
        context.is_currently_error = false;
    }

    return context.is_currently_error ? HistoricalHealthStatus::kError : HistoricalHealthStatus::kOk;
}

}  // namespace score::mw::com::impl::e2e
