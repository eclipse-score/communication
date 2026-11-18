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

const HealthTrackerConfiguration kEnabledConfiguration{HealthTrackerConfiguration::CreateEnabled(3U, 5U, 7U)};

const HealthTrackerConfiguration kDisabledConfiguration{HealthTrackerConfiguration::CreateDisabled()};

HistoricalHealthStatus UpdateWithFailedSample(HealthTracker& tracker)
{
    return tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kDisabled);
}

HistoricalHealthStatus UpdateWithPassedSample(HealthTracker& tracker)
{
    return tracker.Update(DataIntegrityStatus::kOk, SequenceStatus::kOk);
}

HistoricalHealthStatus UpdateWithFailedSamples(const std::size_t count, HealthTracker& tracker)
{
    HistoricalHealthStatus status{HistoricalHealthStatus::kDisabled};
    for (std::size_t i = 0U; i < count; ++i)
    {
        status = UpdateWithFailedSample(tracker);
    }
    return status;
}

HistoricalHealthStatus UpdateWithPassedSamples(const std::size_t count, HealthTracker& tracker)
{
    HistoricalHealthStatus status{HistoricalHealthStatus::kDisabled};
    for (std::size_t i = 0U; i < count; ++i)
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
    // (the iteration count is arbitrary: a disabled configuration never reads a window size)
    constexpr std::uint8_t kIterationCount{7U};
    HistoricalHealthStatus status{HistoricalHealthStatus::kOk};
    for (std::uint8_t i = 0U; i < kIterationCount; ++i)
    {
        status = tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kErrorRepeated);

        // Then the status is always kDisabled
        EXPECT_EQ(status, HistoricalHealthStatus::kDisabled);
    }
}

TEST(E2eHealthTrackerTest, SampleWithBothChecksDisabledReportsDisabledAndDoesNotAdvanceWindow)
{
    // Given a health tracker with an enabled configuration
    HealthTracker tracker{kEnabledConfiguration};

    // When updating the historical health with a sample for which both checks are disabled
    const auto status = tracker.Update(DataIntegrityStatus::kDisabled, SequenceStatus::kDisabled);

    // Then the status is kDisabled
    EXPECT_EQ(status, HistoricalHealthStatus::kDisabled);

    // And the sample did not count as passed, so the error threshold is still reached by the same number of failures
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.GetErrorThreshold() - 1U, tracker),
              HistoricalHealthStatus::kOk);
    EXPECT_EQ(UpdateWithFailedSample(tracker), HistoricalHealthStatus::kError);
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

    // When updating the historical health with a single data integrity error and a skipped sequence check
    const auto status = tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kDisabled);

    // Then the status stays kOk as the error threshold is not reached
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, DataIntegrityErrorsWithSkippedSequenceReachErrorThreshold)
{
    // Given a health tracker which has seen one failed sample less than the error threshold, each with a skipped
    // sequence check
    HealthTracker tracker{kEnabledConfiguration};
    for (std::uint8_t i = 0U; i < (kEnabledConfiguration.GetErrorThreshold() - 1U); ++i)
    {
        ASSERT_EQ(tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kSkipped), HistoricalHealthStatus::kOk);
    }

    // When updating with one more such sample
    const auto status = tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kSkipped);

    // Then the status switches to kError, as the data integrity error counts as the failure
    EXPECT_EQ(status, HistoricalHealthStatus::kError);
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
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.GetErrorThreshold() - 1U, tracker),
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
    UpdateWithPassedSamples(kEnabledConfiguration.GetWindowSize(), tracker);

    // When updating the historical health with two more failed samples
    const auto status = UpdateWithFailedSamples(2U, tracker);

    // Then the status stays kOk as only two failed samples are left in the window
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, FewerPassedSamplesThanRecoveryThresholdDoNotRecover)
{
    // Given a health tracker which has switched to the error state
    HealthTracker tracker{kEnabledConfiguration};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.GetErrorThreshold(), tracker),
              HistoricalHealthStatus::kError);

    // When updating the historical health with one passed sample less than the recovery threshold
    const auto status = UpdateWithPassedSamples(kEnabledConfiguration.GetRecoveryThreshold() - 1U, tracker);

    // Then the status stays kError
    EXPECT_EQ(status, HistoricalHealthStatus::kError);
}

TEST(E2eHealthTrackerTest, PassedSamplesReachingRecoveryThresholdSwitchBackToOk)
{
    // Given a health tracker which has switched to the error state
    HealthTracker tracker{kEnabledConfiguration};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.GetErrorThreshold(), tracker),
              HistoricalHealthStatus::kError);

    // When updating the historical health with as many passed samples as the recovery threshold
    const auto status = UpdateWithPassedSamples(kEnabledConfiguration.GetRecoveryThreshold(), tracker);

    // Then the status switches back to kOk
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, NonConsecutivePassedSamplesWithinWindowSwitchBackToOk)
{
    // Given a health tracker which has switched to the error state and then seen passed, passed, failed, passed,
    // passed samples (four passed samples, one less than the recovery threshold)
    HealthTracker tracker{kEnabledConfiguration};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.GetErrorThreshold(), tracker),
              HistoricalHealthStatus::kError);
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
    UpdateWithFailedSamples(kEnabledConfiguration.GetErrorThreshold(), tracker);
    ASSERT_EQ(UpdateWithPassedSamples(kEnabledConfiguration.GetRecoveryThreshold(), tracker),
              HistoricalHealthStatus::kOk);

    // When updating the historical health with a single failed sample
    const auto status = UpdateWithFailedSample(tracker);

    // Then the status stays kOk as the failed samples left in the window are below the error threshold
    EXPECT_EQ(status, HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, ThresholdsAreRelativeToWindowAndCanDifferFromEachOther)
{
    // Given a configuration with an error threshold of 5, a recovery threshold of 10 and a window of 10 samples
    const HealthTrackerConfiguration kConfiguration{HealthTrackerConfiguration::CreateEnabled(5U, 10U, 10U)};
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

TEST(E2eHealthTrackerTest, MaximumWindowSizeSwitchesToErrorAndBackToOk)
{
    // Given a configuration whose window uses all bits of the sample history
    const HealthTrackerConfiguration kConfiguration{
        HealthTrackerConfiguration::CreateEnabled(33U, 32U, kMaxHealthWindowSize)};
    HealthTracker tracker{kConfiguration};

    // When updating the historical health with one failed sample less than the error threshold
    // Then the status stays kOk
    EXPECT_EQ(UpdateWithFailedSamples(kConfiguration.GetErrorThreshold() - 1U, tracker), HistoricalHealthStatus::kOk);

    // When updating the historical health with the failed sample reaching the error threshold
    // Then the status switches to kError
    EXPECT_EQ(UpdateWithFailedSample(tracker), HistoricalHealthStatus::kError);

    // When updating the historical health with one passed sample less than the recovery threshold
    // Then the status stays kError
    EXPECT_EQ(UpdateWithPassedSamples(kConfiguration.GetRecoveryThreshold() - 1U, tracker),
              HistoricalHealthStatus::kError);

    // When updating the historical health with the passed sample reaching the recovery threshold
    // Then the status switches back to kOk
    EXPECT_EQ(UpdateWithPassedSample(tracker), HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, ResetLeavesErrorStateAndStartsWithEmptyWindow)
{
    // Given a health tracker in state kError
    HealthTracker tracker{kEnabledConfiguration};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.GetErrorThreshold(), tracker),
              HistoricalHealthStatus::kError);

    // When resetting the tracker
    tracker.Reset();

    // Then one passed sample reports kOk, i.e. the state is not kError any more
    EXPECT_EQ(UpdateWithPassedSample(tracker), HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, ResetDiscardsFailedSamplesOfTheWindow)
{
    // Given a health tracker which is one failed sample away from kError
    HealthTracker tracker{kEnabledConfiguration};
    ASSERT_EQ(UpdateWithFailedSamples(kEnabledConfiguration.GetErrorThreshold() - 1U, tracker),
              HistoricalHealthStatus::kOk);

    // When resetting the tracker and updating it with one failed sample
    tracker.Reset();

    // Then the earlier failed samples are not counted and the status stays kOk
    EXPECT_EQ(UpdateWithFailedSample(tracker), HistoricalHealthStatus::kOk);
}

TEST(E2eHealthTrackerTest, ResetKeepsDisabledTrackerDisabled)
{
    // Given a disabled health tracker
    HealthTracker tracker{kDisabledConfiguration};

    // When resetting the tracker and updating it with a failed sample
    tracker.Reset();

    // Then the status is still kDisabled
    EXPECT_EQ(UpdateWithFailedSample(tracker), HistoricalHealthStatus::kDisabled);
}

TEST(E2eHealthTrackerTest, DefaultConfigurationIsDisabledAndWellFormed)
{
    // Given a default-constructed configuration
    constexpr HealthTrackerConfiguration kConfiguration{};

    // When constructing a health tracker from it and updating it with a failing sample
    HealthTracker tracker{kConfiguration};
    const auto status = tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kErrorRepeated);

    // Then construction does not terminate and the status is kDisabled
    EXPECT_EQ(status, HistoricalHealthStatus::kDisabled);
}

TEST(E2eHealthTrackerTest, DisabledConfigurationIsNotValidated)
{
    // Given a configuration created via CreateDisabled()
    const HealthTrackerConfiguration kConfiguration{HealthTrackerConfiguration::CreateDisabled()};

    // When constructing a health tracker from it and updating it with a failing sample
    HealthTracker tracker{kConfiguration};
    const auto status = tracker.Update(DataIntegrityStatus::kError, SequenceStatus::kErrorRepeated);

    // Then construction does not terminate and the status is kDisabled
    EXPECT_EQ(status, HistoricalHealthStatus::kDisabled);
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
    // When creating an enabled configuration from it
    // Then the program terminates
    EXPECT_DEATH(HealthTrackerConfiguration::CreateEnabled(0U, 5U, 8U), ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsZeroRecoveryThreshold)
{
    // Given a health tracker configuration with a recovery threshold of zero
    // When creating an enabled configuration from it
    // Then the program terminates
    EXPECT_DEATH(HealthTrackerConfiguration::CreateEnabled(3U, 0U, 8U), ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsWindowSmallerThanErrorThreshold)
{
    // Given a health tracker configuration whose window is smaller than the error threshold
    // When creating an enabled configuration from it
    // Then the program terminates
    EXPECT_DEATH(HealthTrackerConfiguration::CreateEnabled(6U, 5U, 5U), ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsWindowSmallerThanRecoveryThreshold)
{
    // Given a health tracker configuration whose window is smaller than the recovery threshold
    // When creating an enabled configuration from it
    // Then the program terminates
    EXPECT_DEATH(HealthTrackerConfiguration::CreateEnabled(3U, 6U, 5U), ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsWindowLargerThanMaximum)
{
    // Given a health tracker configuration whose window exceeds the maximum supported window size
    // When creating an enabled configuration from it
    // Then the program terminates
    EXPECT_DEATH(HealthTrackerConfiguration::CreateEnabled(40U, 30U, kMaxHealthWindowSize + 1U), ".*");
}

TEST(E2eHealthTrackerDeathTest, ConstructionRejectsThresholdsWhichAllowOscillation)
{
    // Given a health tracker configuration whose thresholds add up to exactly the window size, so that the state
    // could oscillate between kOk and kError
    // When creating an enabled configuration from it
    // Then the program terminates
    EXPECT_DEATH(HealthTrackerConfiguration::CreateEnabled(3U, 5U, 8U), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::e2e
