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
                                                           /* window_size */ 7U};

constexpr HealthTrackerConfiguration kDisabledConfiguration{/* enabled */ false,
                                                            /* error_threshold */ 3U,
                                                            /* recovery_threshold */ 5U,
                                                            /* window_size */ 7U};

HistoricalHealthStatus UpdateWithFailedSample(HealthTracker& tracker)
{
    return tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kOk);
}

HistoricalHealthStatus UpdateWithPassedSample(HealthTracker& tracker)
{
    return tracker.Update(DataIntegrityStatus::kOk, SequenceStatus::kOk);
}

HistoricalHealthStatus UpdateWithFailedSamples(const std::uint8_t count, HealthTracker& tracker)
{
    HistoricalHealthStatus status{HistoricalHealthStatus::kDisabled};
    for (std::uint8_t i = 0U; i < count; ++i)
    {
        status = UpdateWithFailedSample(tracker);
    }
    return status;
}

HistoricalHealthStatus UpdateWithPassedSamples(const std::uint8_t count, HealthTracker& tracker)
{
    HistoricalHealthStatus status{HistoricalHealthStatus::kDisabled};
    for (std::uint8_t i = 0U; i < count; ++i)
    {
        status = UpdateWithPassedSample(tracker);
    }
    return status;
}

TEST(E2eHealthTrackerTest, DisabledConfigurationAlwaysReportsDisabled)
{
    // Given a health tracker with a disabled configuration
    HealthTracker tracker{kDisabledConfiguration};

    // When updating the historical health repeatedly with failing data integrity and sequence statuses
    HistoricalHealthStatus status{HistoricalHealthStatus::kOk};
    for (std::uint8_t i = 0U; i < kDisabledConfiguration.window_size; ++i)
    {
        status = tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kErrorRepeated);

        // Then the status is always kDisabled
        EXPECT_EQ(status, HistoricalHealthStatus::kDisabled);
    }
}

TEST(E2eHealthTrackerTest, FirstValidSampleReportsOk)
{
    // Given a fresh health tracker with an enabled configuration
    HealthTracker tracker{kEnabledConfiguration};

    // When updating the historical health with a valid sample
    const auto status = tracker.Update(DataIntegrityStatus::kOk, SequenceStatus::kOk);

    // Then the status is kOk
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, SingleDataIntegrityErrorStaysOkBelowErrorThreshold)
{
    // Given a fresh health tracker with an enabled configuration
    HealthTracker tracker{kEnabledConfiguration};

    // When updating the historical health with a single data integrity error
    const auto status = tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kOk);

    // Then the status stays kOk as the error threshold is not reached
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, SingleSequenceGapExceedsThresholdStaysOkBelowErrorThreshold)
{
    // Given a fresh health tracker with an enabled configuration
    HealthTracker tracker{kEnabledConfiguration};

    // When updating the historical health with a single sequence gap exceeding the threshold
    const auto status = tracker.Update(DataIntegrityStatus::kOk, SequenceStatus::kErrorGapExceedsThreshold);

    // Then the status stays kOk as the error threshold is not reached
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, FailedSamplesReachingErrorThresholdSwitchToError)
{
    // Given a health tracker which has seen one failed sample less than the error threshold
    HealthTracker tracker{kEnabledConfiguration};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.error_threshold - 1U, tracker),
              HistoricalHealthStatus::kOk);

    // When updating the historical health with one more failed sample
    const auto status = UpdateWithFailedSample(tracker);

    // Then the status switches to kError
    EXPECT_EQ(status, HistoricalHealthStatus::kError);
}

TEST(E2eHealthTrackerTest, NonConsecutiveFailedSamplesWithinWindowSwitchToError)
{
    // Given a health tracker which has seen failed, passed, failed, passed samples
    HealthTracker tracker{kEnabledConfiguration};
    UpdateWithFailedSample(tracker);
    UpdateWithPassedSample(tracker);
    UpdateWithFailedSample(tracker);
    ASSERT_EQ(UpdateWithPassedSample(tracker), HistoricalHealthStatus::kOk);

    // When updating the historical health with a third failed sample within the window
    const auto status = UpdateWithFailedSample(tracker);

    // Then the status switches to kError
    EXPECT_EQ(status, HistoricalHealthStatus::kError);
}

TEST(E2eHealthTrackerTest, FailedSamplesPushedOutOfWindowAreForgotten)
{
    // Given a health tracker which has seen two failed samples followed by a full window of passed samples
    HealthTracker tracker{kEnabledConfiguration};
    UpdateWithFailedSamples(2U, tracker);
    UpdateWithPassedSamples(kEnabledConfiguration.window_size, tracker);

    // When updating the historical health with two more failed samples
    const auto status = UpdateWithFailedSamples(2U, tracker);

    // Then the status stays kOk as only two failed samples are left in the window
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, FewerPassedSamplesThanRecoveryThresholdDoNotRecover)
{
    // Given a health tracker which has switched to the error state
    HealthTracker tracker{kEnabledConfiguration};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.error_threshold, tracker), HistoricalHealthStatus::kError);

    // When updating the historical health with one passed sample less than the recovery threshold
    const auto status = UpdateWithPassedSamples(kEnabledConfiguration.recovery_threshold - 1U, tracker);

    // Then the status stays kError
    EXPECT_EQ(status, HistoricalHealthStatus::kError);
}

TEST(E2eHealthTrackerTest, PassedSamplesReachingRecoveryThresholdSwitchBackToOk)
{
    // Given a health tracker which has switched to the error state
    HealthTracker tracker{kEnabledConfiguration};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.error_threshold, tracker), HistoricalHealthStatus::kError);

    // When updating the historical health with as many passed samples as the recovery threshold
    const auto status = UpdateWithPassedSamples(kEnabledConfiguration.recovery_threshold, tracker);

    // Then the status switches back to kOk
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, NonConsecutivePassedSamplesWithinWindowSwitchBackToOk)
{
    // Given a health tracker which has switched to the error state and then seen passed, passed, failed, passed,
    // passed samples (four passed samples, one less than the recovery threshold)
    HealthTracker tracker{kEnabledConfiguration};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.error_threshold, tracker), HistoricalHealthStatus::kError);
    UpdateWithPassedSamples(2U, tracker);
    UpdateWithFailedSample(tracker);
    ASSERT_EQ(UpdateWithPassedSamples(2U, tracker), HistoricalHealthStatus::kError);

    // When updating the historical health with a fifth passed sample within the window
    const auto status = UpdateWithPassedSample(tracker);

    // Then the status switches back to kOk
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, SwitchingBackToOkDoesNotImmediatelyReturnToError)
{
    // Given a health tracker which has switched to error and back to ok again
    HealthTracker tracker{kEnabledConfiguration};
    UpdateWithFailedSamples(kEnabledConfiguration.error_threshold, tracker);
    ASSERT_EQ(UpdateWithPassedSamples(kEnabledConfiguration.recovery_threshold, tracker), HistoricalHealthStatus::kOk);

    // When updating the historical health with a single failed sample
    const auto status = UpdateWithFailedSample(tracker);

    // Then the status stays kOk as the failed samples left in the window are below the error threshold
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, ThresholdsAreRelativeToWindowAndCanDifferFromEachOther)
{
    // Given a configuration with an error threshold of 5, a recovery threshold of 10 and a window of 10 samples
    constexpr HealthTrackerConfiguration kConfiguration{true, 5U, 10U, 10U};
    HealthTracker tracker{kConfiguration};

    // When updating the historical health with 4 failed samples
    // Then the status stays kOk
    EXPECT_EQ(UpdateWithFailedSamples(4U, tracker), HistoricalHealthStatus::kOk);

    // When updating the historical health with the 5th failed sample
    // Then the status switches to kError
    EXPECT_EQ(UpdateWithFailedSample(tracker), HistoricalHealthStatus::kError);

    // When updating the historical health with 9 passed samples
    // Then the status stays kError
    EXPECT_EQ(UpdateWithPassedSamples(9U, tracker), HistoricalHealthStatus::kError);

    // When updating the historical health with the 10th passed sample
    // Then the status switches back to kOk
    EXPECT_EQ(UpdateWithPassedSample(tracker), HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, ConstructionAcceptsWellFormedConfiguration)
{
    // Given well-formed enabled and disabled health tracker configurations
    // When constructing health trackers from them
    // Then construction does not terminate
    const HealthTracker enabled_tracker{kEnabledConfiguration};
    const HealthTracker disabled_tracker{kDisabledConfiguration};
    static_cast<void>(enabled_tracker);
    static_cast<void>(disabled_tracker);
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsZeroErrorThreshold)
{
    // Given a health tracker configuration with an error threshold of zero
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 0U, 5U, 8U};

    // When constructing a health tracker from it
    // Then the program terminates
    EXPECT_DEATH(HealthTracker{kInvalidConfiguration}, ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsZeroRecoveryThreshold)
{
    // Given a health tracker configuration with a recovery threshold of zero
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 3U, 0U, 8U};

    // When constructing a health tracker from it
    // Then the program terminates
    EXPECT_DEATH(HealthTracker{kInvalidConfiguration}, ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsWindowSmallerThanErrorThreshold)
{
    // Given a health tracker configuration whose window is smaller than the error threshold
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 6U, 5U, 5U};

    // When constructing a health tracker from it
    // Then the program terminates
    EXPECT_DEATH(HealthTracker{kInvalidConfiguration}, ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsWindowSmallerThanRecoveryThreshold)
{
    // Given a health tracker configuration whose window is smaller than the recovery threshold
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 3U, 6U, 5U};

    // When constructing a health tracker from it
    // Then the program terminates
    EXPECT_DEATH(HealthTracker{kInvalidConfiguration}, ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsWindowLargerThanMaximum)
{
    // Given a health tracker configuration whose window exceeds the maximum supported window size
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 40U, 30U, kMaxHealthWindowSize + 1U};

    // When constructing a health tracker from it
    // Then the program terminates
    EXPECT_DEATH(HealthTracker{kInvalidConfiguration}, ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsThresholdsWhichAllowOscillation)
{
    // Given a health tracker configuration whose thresholds add up to exactly the window size, so that the state
    // could oscillate between kOk and kError
    constexpr HealthTrackerConfiguration kInvalidConfiguration{true, 3U, 5U, 8U};

    // When constructing a health tracker from it
    // Then the program terminates
    EXPECT_DEATH(HealthTracker{kInvalidConfiguration}, ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::e2e
