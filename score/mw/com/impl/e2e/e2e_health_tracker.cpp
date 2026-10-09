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

HealthTracker::HealthTracker(const HealthTrackerConfiguration& config) noexcept
    : config_{config}, failed_samples_{0U}, samples_in_window_{0U}, is_currently_error_{false}
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config_.error_threshold >= 1U,
                                                      "error_threshold must be at least 1");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config_.recovery_threshold >= 1U,
                                                      "recovery_threshold must be at least 1");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config_.window_size >= config_.error_threshold,
                                                      "window_size must be at least error_threshold");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config_.window_size >= config_.recovery_threshold,
                                                      "window_size must be at least recovery_threshold");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config_.window_size <= kMaxHealthWindowSize,
                                                      "window_size must not exceed kMaxHealthWindowSize");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        (config_.error_threshold + config_.recovery_threshold) > config_.window_size,
        "error_threshold + recovery_threshold must exceed window_size to avoid oscillation between kOk and kError");
}

HistoricalHealthStatus HealthTracker::Update(const DataIntegrityStatus data_integrity,
                                             const SequenceStatus sequence) noexcept
{
    if (!config_.enabled)
    {
        return HistoricalHealthStatus::kDisabled;
    }

    const bool data_integrity_failed{data_integrity == DataIntegrityStatus::kError};
    const bool sequence_failed{(sequence == SequenceStatus::kErrorRepeated) ||
                               (sequence == SequenceStatus::kErrorGapExceedsThreshold)};
    const bool this_sample_failed{data_integrity_failed || sequence_failed};

    // Newest sample enters at bit 0; samples older than window_size are masked out.
    failed_samples_ = (failed_samples_ << 1U) | (this_sample_failed ? 1U : 0U);
    if (config_.window_size < kMaxHealthWindowSize)
    {
        failed_samples_ &= (std::uint64_t{1U} << config_.window_size) - 1U;
    }
    if (samples_in_window_ < config_.window_size)
    {
        ++samples_in_window_;
    }

    const std::size_t failed_count{std::bitset<kMaxHealthWindowSize>{failed_samples_}.count()};
    const std::size_t passed_count{samples_in_window_ - failed_count};

    const bool state_flips{is_currently_error_ ? (passed_count >= config_.recovery_threshold)
                                               : (failed_count >= config_.error_threshold)};
    if (state_flips)
    {
        is_currently_error_ = !is_currently_error_;
    }

    return is_currently_error_ ? HistoricalHealthStatus::kError : HistoricalHealthStatus::kOk;
}

}  // namespace score::mw::com::impl::e2e
