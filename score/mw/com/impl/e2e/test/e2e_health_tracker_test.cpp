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
                                                           /* recovery_threshold */ 5U,
                                                           /* window_size */ 8U};

constexpr HealthTrackerConfiguration kDisabledConfiguration{/* enabled */ false,
                                                            /* error_threshold */ 3U,
                                                            /* recovery_threshold */ 5U,
                                                            /* window_size */ 8U};

HistoricalHealthStatus UpdateWithFailedSample(const HealthTrackerConfiguration& config, HealthContext& context)
{
    return UpdateHistoricalHealth(DataIntegrityStatus::kError, SequenceStatus::kOk, config, context);
}

HistoricalHealthStatus UpdateWithPassedSample(const HealthTrackerConfiguration& config, HealthContext& context)
{
    return UpdateHistoricalHealth(DataIntegrityStatus::kOk, SequenceStatus::kOk, config, context);
}

HistoricalHealthStatus UpdateWithFailedSamples(const std::uint8_t count,
                                               const HealthTrackerConfiguration& config,
                                               HealthContext& context)
{
    HistoricalHealthStatus status{HistoricalHealthStatus::kDisabled};
    for (std::uint8_t i = 0U; i < count; ++i)
    {
        status = UpdateWithFailedSample(config, context);
    }
    return status;
}

HistoricalHealthStatus UpdateWithPassedSamples(const std::uint8_t count,
                                               const HealthTrackerConfiguration& config,
                                               HealthContext& context)
{
    HistoricalHealthStatus status{HistoricalHealthStatus::kDisabled};
    for (std::uint8_t i = 0U; i < count; ++i)
    {
        status = UpdateWithPassedSample(config, context);
    }
    return status;
}

TEST(E2eHealthTrackerTest, DisabledConfigurationAlwaysReportsDisabledAndDoesNotTouchContext)
{
    // Given a fresh health context and a disabled health tracker configuration
    HealthContext context{};

    // When updating the historical health with failing data integrity and sequence statuses
    const auto status = UpdateHistoricalHealth(
        DataIntegrityStatus::kError, SequenceStatus::kErrorRepeated, kDisabledConfiguration, context);

    // Then the status is kDisabled and the context is left untouched
    EXPECT_EQ(status, HistoricalHealthStatus::kDisabled);
    EXPECT_EQ(context.failed_samples, 0U);
    EXPECT_EQ(context.samples_in_window, 0U);
    EXPECT_FALSE(context.is_currently_error);
}

TEST(E2eHealthTrackerTest, FirstValidSampleReportsOk)
{
    // Given a fresh health context and an enabled health tracker configuration
    HealthContext context{};

    // When updating the historical health with a valid sample
    const auto status =
        UpdateHistoricalHealth(DataIntegrityStatus::kOk, SequenceStatus::kOk, kEnabledConfiguration, context);

    // Then the status is kOk
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
    EXPECT_FALSE(context.is_currently_error);
}

TEST(E2eHealthTrackerTest, SingleDataIntegrityErrorStaysOkBelowErrorThreshold)
{
    // Given a fresh health context and an enabled health tracker configuration
    HealthContext context{};

    // When updating the historical health with a single data integrity error
    const auto status =
        UpdateHistoricalHealth(DataIntegrityStatus::kError, SequenceStatus::kOk, kEnabledConfiguration, context);

    // Then the status stays kOk as the error threshold is not reached
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, SingleSequenceGapExceedsThresholdStaysOkBelowErrorThreshold)
{
    // Given a fresh health context and an enabled health tracker configuration
    HealthContext context{};

    // When updating the historical health with a single sequence gap exceeding the threshold
    const auto status = UpdateHistoricalHealth(
        DataIntegrityStatus::kOk, SequenceStatus::kErrorGapExceedsThreshold, kEnabledConfiguration, context);

    // Then the status stays kOk as the error threshold is not reached
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, FailedSamplesReachingErrorThresholdSwitchToError)
{
    // Given a health context which has seen one failed sample less than the error threshold
    HealthContext context{};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.error_threshold - 1U, kEnabledConfiguration, context),
              HistoricalHealthStatus::kOk);

    // When updating the historical health with one more failed sample
    const auto status = UpdateWithFailedSample(kEnabledConfiguration, context);

    // Then the status switches to kError
    EXPECT_EQ(status, HistoricalHealthStatus::kError);
}

TEST(E2eHealthTrackerTest, NonConsecutiveFailedSamplesWithinWindowSwitchToError)
{
    // Given a health context which has seen failed, passed, failed, passed samples
    HealthContext context{};
    UpdateWithFailedSample(kEnabledConfiguration, context);
    UpdateWithPassedSample(kEnabledConfiguration, context);
    UpdateWithFailedSample(kEnabledConfiguration, context);
    ASSERT_EQ(UpdateWithPassedSample(kEnabledConfiguration, context), HistoricalHealthStatus::kOk);

    // When updating the historical health with a third failed sample within the window
    const auto status = UpdateWithFailedSample(kEnabledConfiguration, context);

    // Then the status switches to kError
    EXPECT_EQ(status, HistoricalHealthStatus::kError);
}

TEST(E2eHealthTrackerTest, FailedSamplesPushedOutOfWindowAreForgotten)
{
    // Given a health context which has seen two failed samples followed by a full window of passed samples
    HealthContext context{};
    UpdateWithFailedSamples(2U, kEnabledConfiguration, context);
    UpdateWithPassedSamples(kEnabledConfiguration.window_size, kEnabledConfiguration, context);

    // When updating the historical health with two more failed samples
    const auto status = UpdateWithFailedSamples(2U, kEnabledConfiguration, context);

    // Then the status stays kOk as only two failed samples are left in the window
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, FewerPassedSamplesThanRecoveryThresholdDoNotRecover)
{
    // Given a health context which has switched to the error state
    HealthContext context{};
    UpdateWithFailedSamples(kEnabledConfiguration.error_threshold, kEnabledConfiguration, context);
    ASSERT_TRUE(context.is_currently_error);

    // When updating the historical health with one passed sample less than the recovery threshold
    const auto status =
        UpdateWithPassedSamples(kEnabledConfiguration.recovery_threshold - 1U, kEnabledConfiguration, context);

    // Then the status stays kError
    EXPECT_EQ(status, HistoricalHealthStatus::kError);
}

TEST(E2eHealthTrackerTest, PassedSamplesReachingRecoveryThresholdSwitchBackToOk)
{
    // Given a health context which has switched to the error state
    HealthContext context{};
    UpdateWithFailedSamples(kEnabledConfiguration.error_threshold, kEnabledConfiguration, context);
    ASSERT_TRUE(context.is_currently_error);

    // When updating the historical health with as many passed samples as the recovery threshold
    const auto status =
        UpdateWithPassedSamples(kEnabledConfiguration.recovery_threshold, kEnabledConfiguration, context);

    // Then the status switches back to kOk
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, NonConsecutivePassedSamplesWithinWindowSwitchBackToOk)
{
    // Given a health context which has switched to the error state and then seen passed, passed, failed, passed,
    // passed samples (four passed samples, one less than the recovery threshold)
    HealthContext context{};
    UpdateWithFailedSamples(kEnabledConfiguration.error_threshold, kEnabledConfiguration, context);
    ASSERT_TRUE(context.is_currently_error);
    UpdateWithPassedSamples(2U, kEnabledConfiguration, context);
    UpdateWithFailedSample(kEnabledConfiguration, context);
    ASSERT_EQ(UpdateWithPassedSamples(2U, kEnabledConfiguration, context), HistoricalHealthStatus::kError);

    // When updating the historical health with a fifth passed sample within the window
    const auto status = UpdateWithPassedSample(kEnabledConfiguration, context);

    // Then the status switches back to kOk
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, SwitchingBackToOkClearsWindowSoOldFailuresDoNotRetriggerError)
{
    // Given a health context which has switched to error and back to ok again
    HealthContext context{};
    UpdateWithFailedSamples(kEnabledConfiguration.error_threshold, kEnabledConfiguration, context);
    UpdateWithPassedSamples(kEnabledConfiguration.recovery_threshold, kEnabledConfiguration, context);
    ASSERT_FALSE(context.is_currently_error);

    // When updating the historical health with a single failed sample
    const auto status = UpdateWithFailedSample(kEnabledConfiguration, context);

    // Then the status stays kOk
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, ThresholdsAreRelativeToWindowAndCanDifferFromEachOther)
{
    // Given a configuration with an error threshold of 5, a recovery threshold of 10 and a window of 10 samples
    constexpr HealthTrackerConfiguration kConfiguration{true, 5U, 10U, 10U};
    HealthContext context{};

    // When updating the historical health with 4 failed samples
    // Then the status stays kOk
    EXPECT_EQ(UpdateWithFailedSamples(4U, kConfiguration, context), HistoricalHealthStatus::kOk);

    // When updating the historical health with the 5th failed sample
    // Then the status switches to kError
    EXPECT_EQ(UpdateWithFailedSample(kConfiguration, context), HistoricalHealthStatus::kError);

    // When updating the historical health with 9 passed samples
    // Then the status stays kError
    EXPECT_EQ(UpdateWithPassedSamples(9U, kConfiguration, context), HistoricalHealthStatus::kError);

    // When updating the historical health with the 10th passed sample
    // Then the status switches back to kOk
    EXPECT_EQ(UpdateWithPassedSample(kConfiguration, context), HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, ValidateHealthTrackerConfigurationAcceptsWellFormedConfiguration)
{
    // Given well-formed enabled and disabled health tracker configurations
    // When validating them
    // Then validation does not terminate
    ValidateHealthTrackerConfiguration(kEnabledConfiguration);
    ValidateHealthTrackerConfiguration(kDisabledConfiguration);
}

TEST(E2eHealthTrackerDeathTest, ValidateHealthTrackerConfigurationRejectsZeroErrorThreshold)
{
    // Given a health tracker configuration with an error threshold of zero
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 0U, 5U, 8U};

    // When validating the configuration
    // Then the program terminates
    EXPECT_DEATH(ValidateHealthTrackerConfiguration(kInvalidConfiguration), ".*");
}

TEST(E2eHealthTrackerDeathTest, ValidateHealthTrackerConfigurationRejectsZeroRecoveryThreshold)
{
    // Given a health tracker configuration with a recovery threshold of zero
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 3U, 0U, 8U};

    // When validating the configuration
    // Then the program terminates
    EXPECT_DEATH(ValidateHealthTrackerConfiguration(kInvalidConfiguration), ".*");
}

TEST(E2eHealthTrackerDeathTest, ValidateHealthTrackerConfigurationRejectsWindowSmallerThanErrorThreshold)
{
    // Given a health tracker configuration whose window is smaller than the error threshold
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 6U, 5U, 5U};

    // When validating the configuration
    // Then the program terminates
    EXPECT_DEATH(ValidateHealthTrackerConfiguration(kInvalidConfiguration), ".*");
}

TEST(E2eHealthTrackerDeathTest, ValidateHealthTrackerConfigurationRejectsWindowSmallerThanRecoveryThreshold)
{
    // Given a health tracker configuration whose window is smaller than the recovery threshold
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 3U, 6U, 5U};

    // When validating the configuration
    // Then the program terminates
    EXPECT_DEATH(ValidateHealthTrackerConfiguration(kInvalidConfiguration), ".*");
}

TEST(E2eHealthTrackerDeathTest, ValidateHealthTrackerConfigurationRejectsWindowLargerThanMaximum)
{
    // Given a health tracker configuration whose window exceeds the maximum supported window size
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 3U, 5U, kMaxHealthWindowSize + 1U};

    // When validating the configuration
    // Then the program terminates
    EXPECT_DEATH(ValidateHealthTrackerConfiguration(kInvalidConfiguration), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::e2e
