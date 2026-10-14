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
    // A disabled tracker never reads the thresholds or the window, so they are only validated when enabled.
    if (!config_.enabled)
    {
        return;
    }

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
    // Without an enabled tracker or without any executed check there is nothing to judge, so the sample must neither
    // be counted as passed nor as failed.
    if (!config_.enabled ||
        ((data_integrity == DataIntegrityStatus::kDisabled) && (sequence == SequenceStatus::kDisabled)))
    {
        return HistoricalHealthStatus::kDisabled;
    }

    const bool this_sample_failed{IsFailure(data_integrity) || IsFailure(sequence)};

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

void HealthTracker::Reset() noexcept
{
    failed_samples_ = 0U;
    samples_in_window_ = 0U;
    is_currently_error_ = false;
}

}  // namespace score::mw::com::impl::e2e
