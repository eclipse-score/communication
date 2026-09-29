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

#include <gtest/gtest.h>

namespace score::mw::com::impl::e2e
{
namespace
{

constexpr HealthTrackerConfiguration kEnabledConfiguration{/* enabled */ true,
                                                            /* error_threshold */ 3U,
                                                            /* recovery_threshold */ 1U};

constexpr HealthTrackerConfiguration kDisabledConfiguration{/* enabled */ false,
                                                             /* error_threshold */ 3U,
                                                             /* recovery_threshold */ 1U};

TEST(E2eHealthTrackerTest, DisabledConfigurationAlwaysReportsDisabledAndDoesNotTouchContext)
{
    HealthContext context{};

    const auto status =
        UpdateHistoricalHealth(DataIntegrityStatus::kError, SequenceStatus::kErrorRepeated, kDisabledConfiguration, context);

    EXPECT_EQ(status, HistoricalHealthStatus::kDisabled);
    EXPECT_EQ(context.error_counter, 0U);
    EXPECT_FALSE(context.is_currently_error);
}

TEST(E2eHealthTrackerTest, FirstValidSampleReportsOk)
{
    HealthContext context{};

    const auto status =
        UpdateHistoricalHealth(DataIntegrityStatus::kOk, SequenceStatus::kOk, kEnabledConfiguration, context);

    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
    EXPECT_EQ(context.error_counter, 0U);
}

TEST(E2eHealthTrackerTest, DataIntegrityErrorIncrementsCounterButStaysOkBelowThreshold)
{
    HealthContext context{};

    const auto status =
        UpdateHistoricalHealth(DataIntegrityStatus::kError, SequenceStatus::kOk, kEnabledConfiguration, context);

    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
    EXPECT_EQ(context.error_counter, 1U);
}

TEST(E2eHealthTrackerTest, SequenceGapExceedsThresholdIncrementsCounterButStaysOkBelowThreshold)
{
    HealthContext context{};

    const auto status = UpdateHistoricalHealth(
        DataIntegrityStatus::kOk, SequenceStatus::kErrorGapExceedsThreshold, kEnabledConfiguration, context);

    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
    EXPECT_EQ(context.error_counter, 1U);
}

TEST(E2eHealthTrackerTest, RepeatedFailuresReachingErrorThresholdLatchIntoError)
{
    HealthContext context{};
    HistoricalHealthStatus status{HistoricalHealthStatus::kDisabled};

    for (std::uint8_t i = 0U; i < kEnabledConfiguration.error_threshold; ++i)
    {
        status = UpdateHistoricalHealth(DataIntegrityStatus::kError, SequenceStatus::kOk, kEnabledConfiguration, context);
    }

    EXPECT_EQ(status, HistoricalHealthStatus::kError);
    EXPECT_EQ(context.error_counter, kEnabledConfiguration.error_threshold);
}

TEST(E2eHealthTrackerTest, SingleGoodSampleAfterLatchingErrorDoesNotYetRecover)
{
    HealthContext context{};
    for (std::uint8_t i = 0U; i < kEnabledConfiguration.error_threshold; ++i)
    {
        UpdateHistoricalHealth(DataIntegrityStatus::kError, SequenceStatus::kOk, kEnabledConfiguration, context);
    }
    ASSERT_TRUE(context.is_currently_error);

    const auto status =
        UpdateHistoricalHealth(DataIntegrityStatus::kOk, SequenceStatus::kOk, kEnabledConfiguration, context);

    // error_counter drops from 3 to 2, which is still above recovery_threshold (1), so it stays latched.
    EXPECT_EQ(status, HistoricalHealthStatus::kError);
    EXPECT_EQ(context.error_counter, 2U);
}

TEST(E2eHealthTrackerTest, EnoughGoodSamplesAfterErrorRecoverToOk)
{
    HealthContext context{};
    for (std::uint8_t i = 0U; i < kEnabledConfiguration.error_threshold; ++i)
    {
        UpdateHistoricalHealth(DataIntegrityStatus::kError, SequenceStatus::kOk, kEnabledConfiguration, context);
    }
    ASSERT_TRUE(context.is_currently_error);

    HistoricalHealthStatus status{HistoricalHealthStatus::kDisabled};
    // error_counter: 3 -> 2 -> 1 (== recovery_threshold, latches back to kOk).
    for (std::uint8_t i = 0U; i < 2U; ++i)
    {
        status = UpdateHistoricalHealth(DataIntegrityStatus::kOk, SequenceStatus::kOk, kEnabledConfiguration, context);
    }

    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
    EXPECT_EQ(context.error_counter, kEnabledConfiguration.recovery_threshold);
}

TEST(E2eHealthTrackerTest, ErrorCounterDoesNotUnderflowBelowZero)
{
    HealthContext context{};

    for (std::uint8_t i = 0U; i < 5U; ++i)
    {
        const auto status =
            UpdateHistoricalHealth(DataIntegrityStatus::kOk, SequenceStatus::kOk, kEnabledConfiguration, context);

        EXPECT_EQ(status, HistoricalHealthStatus::kOk);
        EXPECT_EQ(context.error_counter, 0U);
    }
}

TEST(E2eHealthTrackerTest, ValidateHealthTrackerConfigurationAcceptsWellFormedConfiguration)
{
    ValidateHealthTrackerConfiguration(kEnabledConfiguration);
    ValidateHealthTrackerConfiguration(kDisabledConfiguration);
}

TEST(E2eHealthTrackerDeathTest, ValidateHealthTrackerConfigurationRejectsZeroErrorThreshold)
{
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 0U, 0U};

    EXPECT_DEATH(ValidateHealthTrackerConfiguration(kInvalidConfiguration), ".*");
}

TEST(E2eHealthTrackerDeathTest, ValidateHealthTrackerConfigurationRejectsRecoveryThresholdNotBelowErrorThreshold)
{
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 3U, 3U};

    EXPECT_DEATH(ValidateHealthTrackerConfiguration(kInvalidConfiguration), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::e2e
