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

#include <algorithm>
#include <bitset>
#include <cstddef>

namespace score::mw::com::impl::e2e
{

HealthTracker::HealthTracker(const HealthTrackerConfiguration& config) noexcept
    : config_{config}, failed_samples_{0U}, passed_samples_{0U}, samples_in_window_{0U}, is_currently_error_{false}
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
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        (config_.min_ok_threshold + config_.error_threshold) <= (config_.window_size + 1),
        "min_ok_threshold must not exceed window_size - error_threshold + 1");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(config_.reception_timeout.count() >= 0,
                                                      "reception_timeout must not be negative");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        (config_.reception_timeout.count() == 0) || (config_.min_ok_threshold > 0U),
        "reception_timeout requires min_ok_threshold > 0, otherwise missing data has no effect");
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
    PushSlot(!this_sample_failed, this_sample_failed);
    UpdateState();
    return GetStatus();
}

HistoricalHealthStatus HealthTracker::RecordNoNewData(const std::uint8_t count) noexcept
{
    if (!config_.enabled)
    {
        return HistoricalHealthStatus::kDisabled;
    }

    // More than window_size neutral slots leave the same window as exactly window_size.
    const std::uint8_t slots{std::min(count, config_.window_size)};
    for (std::uint8_t i = 0U; i < slots; ++i)
    {
        PushSlot(false, false);
        UpdateState();
    }
    return GetStatus();
}

HistoricalHealthStatus HealthTracker::GetStatus() const noexcept
{
    if (!config_.enabled)
    {
        return HistoricalHealthStatus::kDisabled;
    }
    return is_currently_error_ ? HistoricalHealthStatus::kError : HistoricalHealthStatus::kOk;
}

void HealthTracker::PushSlot(const bool passed, const bool failed) noexcept
{
    // Newest slot enters at bit 0; slots older than window_size are masked out.
    failed_samples_ = (failed_samples_ << 1U) | (failed ? 1U : 0U);
    passed_samples_ = (passed_samples_ << 1U) | (passed ? 1U : 0U);
    if (config_.window_size < kMaxHealthWindowSize)
    {
        const std::uint64_t window_mask{(std::uint64_t{1U} << config_.window_size) - 1U};
        failed_samples_ &= window_mask;
        passed_samples_ &= window_mask;
    }
    if (samples_in_window_ < config_.window_size)
    {
        ++samples_in_window_;
    }
}

void HealthTracker::UpdateState() noexcept
{
    const std::size_t failed_count{std::bitset<kMaxHealthWindowSize>{failed_samples_}.count()};
    const std::size_t passed_count{std::bitset<kMaxHealthWindowSize>{passed_samples_}.count()};

    // Only judged on a full window, so the first slots after (re)subscribe cannot trigger it.
    const bool too_few_passed{(samples_in_window_ == config_.window_size) && (passed_count < config_.min_ok_threshold)};

    const bool state_flips{is_currently_error_ ? (passed_count >= config_.recovery_threshold)
                                               : ((failed_count >= config_.error_threshold) || too_few_passed)};
    if (state_flips)
    {
        is_currently_error_ = !is_currently_error_;
    }
}

void HealthTracker::Reset() noexcept
{
    failed_samples_ = 0U;
    passed_samples_ = 0U;
    samples_in_window_ = 0U;
    is_currently_error_ = false;
}

}  // namespace score::mw::com::impl::e2e
