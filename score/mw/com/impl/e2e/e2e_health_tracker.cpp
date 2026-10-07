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

#include <bitset>
#include <cstddef>

namespace score::mw::com::impl::e2e
{

void ValidateHealthTrackerConfiguration(const HealthTrackerConfiguration& config) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config.error_threshold >= 1U,
                                                      "error_threshold must be at least 1");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config.recovery_threshold >= 1U,
                                                      "recovery_threshold must be at least 1");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config.window_size >= config.error_threshold,
                                                      "window_size must be at least error_threshold");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config.window_size >= config.recovery_threshold,
                                                      "window_size must be at least recovery_threshold");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config.window_size <= kMaxHealthWindowSize,
                                                      "window_size must not exceed kMaxHealthWindowSize");
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

    // Newest sample enters at bit 0; samples older than window_size are masked out.
    context.failed_samples = (context.failed_samples << 1U) | (this_sample_failed ? 1U : 0U);
    if (config.window_size < kMaxHealthWindowSize)
    {
        context.failed_samples &= (std::uint64_t{1U} << config.window_size) - 1U;
    }
    if (context.samples_in_window < config.window_size)
    {
        ++context.samples_in_window;
    }

    const std::size_t failed_count{std::bitset<kMaxHealthWindowSize>{context.failed_samples}.count()};
    const std::size_t passed_count{context.samples_in_window - failed_count};

    const bool state_flips{context.is_currently_error ? (passed_count >= config.recovery_threshold)
                                                      : (failed_count >= config.error_threshold)};
    if (state_flips)
    {
        context.is_currently_error = !context.is_currently_error;
        // The samples which caused the flip must not influence the next decision.
        context.failed_samples = 0U;
        context.samples_in_window = 0U;
    }

    return context.is_currently_error ? HistoricalHealthStatus::kError : HistoricalHealthStatus::kOk;
}

}  // namespace score::mw::com::impl::e2e
