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

// --- ComputeSummary --------------------------------------------------------

TEST(E2eSummaryTest, AllDisabledReportsSummaryDisabled)
{
    // Given data integrity, sequence and historical health are all disabled
    // When computing the summary
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kDisabled, SequenceStatus::kDisabled, HistoricalHealthStatus::kDisabled);

    // Then the summary is kDisabled
    EXPECT_EQ(summary, Summary::kDisabled);
}

TEST(E2eSummaryTest, AllEnabledAllPassReportsOk)
{
    // Given data integrity, sequence and historical health are all enabled and passing
    // When computing the summary
    const auto summary = ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kOk, HistoricalHealthStatus::kOk);

    // Then the summary is kOk
    EXPECT_EQ(summary, Summary::kOk);
}

TEST(E2eSummaryTest, AllEnabledWithGapWithinThresholdStillReportsOk)
{
    // Given all checks are enabled and the sequence has a gap within the threshold
    // When computing the summary
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kOkGapWithinThreshold, HistoricalHealthStatus::kOk);

    // Then the summary is kOk
    EXPECT_EQ(summary, Summary::kOk);
}

TEST(E2eSummaryTest, HistoricalHealthDisabledOthersPassReportsOkWithDisabledChecks)
{
    // Given historical health is disabled while data integrity and sequence are passing
    // When computing the summary
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kOk, HistoricalHealthStatus::kDisabled);

    // Then the summary is kOkWithDisabledChecks
    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, SequenceDisabledOthersPassReportsOkWithDisabledChecks)
{
    // Given sequence is disabled while data integrity and historical health are passing
    // When computing the summary
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kDisabled, HistoricalHealthStatus::kOk);

    // Then the summary is kOkWithDisabledChecks
    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, DataIntegrityDisabledOthersPassReportsOkWithDisabledChecks)
{
    // Given data integrity is disabled while sequence and historical health are passing
    // When computing the summary
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kDisabled, SequenceStatus::kOk, HistoricalHealthStatus::kOk);

    // Then the summary is kOkWithDisabledChecks
    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, OnlyDataIntegrityEnabledReportsOkWithDisabledChecks)
{
    // Given only data integrity is enabled and passing
    // When computing the summary
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kDisabled, HistoricalHealthStatus::kDisabled);

    // Then the summary is kOkWithDisabledChecks
    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, OnlySequenceEnabledReportsOkWithDisabledChecks)
{
    // Given only sequence is enabled and passing
    // When computing the summary
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kDisabled, SequenceStatus::kOk, HistoricalHealthStatus::kDisabled);

    // Then the summary is kOkWithDisabledChecks
    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, DataIntegrityErrorReportsError)
{
    // Given data integrity reports an error while sequence and historical health are passing
    // When computing the summary
    const auto summary = ComputeSummary(DataIntegrityStatus::kError, SequenceStatus::kOk, HistoricalHealthStatus::kOk);

    // Then the summary is kError
    EXPECT_EQ(summary, Summary::kError);
}

TEST(E2eSummaryTest, SequenceRepeatedErrorReportsError)
{
    // Given the sequence reports a repeated error while data integrity and historical health are passing
    // When computing the summary
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kErrorRepeated, HistoricalHealthStatus::kOk);

    // Then the summary is kError
    EXPECT_EQ(summary, Summary::kError);
}

TEST(E2eSummaryTest, SequenceGapExceedsThresholdErrorReportsError)
{
    // Given the sequence reports a gap exceeding the threshold while data integrity and historical health are passing
    // When computing the summary
    const auto summary = ComputeSummary(
        DataIntegrityStatus::kOk, SequenceStatus::kErrorGapExceedsThreshold, HistoricalHealthStatus::kOk);

    // Then the summary is kError
    EXPECT_EQ(summary, Summary::kError);
}

TEST(E2eSummaryTest, HistoricalHealthErrorReportsError)
{
    // Given historical health reports an error while data integrity and sequence are passing
    // When computing the summary
    const auto summary = ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kOk, HistoricalHealthStatus::kError);

    // Then the summary is kError
    EXPECT_EQ(summary, Summary::kError);
}

// Doc's misconfiguration Case 5: HistoricalHealthTrackingEnabled=true while both underlying checks
// are disabled forces historical_health to kError even though data_integrity/sequence are
// kDisabled. Error must take priority over the "all disabled" shortcut, so this must be kError,
// not kDisabled.
TEST(E2eSummaryTest, HistoricalHealthErrorWithOtherFieldsDisabledReportsErrorNotDisabled)
{
    // Given historical health reports an error while data integrity and sequence are disabled
    // When computing the summary
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kDisabled, SequenceStatus::kDisabled, HistoricalHealthStatus::kError);

    // Then the summary is kError
    EXPECT_EQ(summary, Summary::kError);
}

TEST(BuildE2EResultTest, BuildE2EResultPopulatesAllFourFieldsConsistently)
{
    // Given a fresh health tracker with an enabled configuration
    HealthTracker tracker{kEnabledConfiguration};

    // When building the E2E result from passing data integrity and sequence statuses
    const auto result = BuildE2EResult(DataIntegrityStatus::kOk, SequenceStatus::kOk, tracker);

    // Then all four fields of the result are populated consistently
    EXPECT_EQ(result.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(result.sequence, SequenceStatus::kOk);
    EXPECT_EQ(result.historical_health, HistoricalHealthStatus::kOk);
    EXPECT_EQ(result.summary, Summary::kOk);
}

TEST(BuildE2EResultTest, BuildE2EResultWithDisabledHealthTrackerStillComputesSummary)
{
    // Given a fresh health tracker with a disabled configuration
    HealthTracker tracker{kDisabledConfiguration};

    // When building the E2E result from passing data integrity and sequence statuses
    const auto result = BuildE2EResult(DataIntegrityStatus::kOk, SequenceStatus::kOk, tracker);

    // Then historical health is kDisabled and the summary is still computed as kOkWithDisabledChecks
    EXPECT_EQ(result.historical_health, HistoricalHealthStatus::kDisabled);
    EXPECT_EQ(result.summary, Summary::kOkWithDisabledChecks);
}

TEST(BuildE2EResultTest, BuildE2EResultWithFailingSampleReportsErrorAndAdvancesHealthTracker)
{
    // Given a fresh health tracker with an enabled configuration
    HealthTracker tracker{kEnabledConfiguration};

    // When building the E2E result from a failing data integrity status
    const auto first_result = BuildE2EResult(DataIntegrityStatus::kError, SequenceStatus::kOk, tracker);

    // Then the summary is kError although the historical health is still below the error threshold
    EXPECT_EQ(first_result.summary, Summary::kError);
    EXPECT_EQ(first_result.historical_health, HistoricalHealthStatus::kOk);

    // And once the error threshold is reached the historical health reports kError as well
    BuildE2EResult(DataIntegrityStatus::kError, SequenceStatus::kOk, tracker);
    const auto third_result = BuildE2EResult(DataIntegrityStatus::kError, SequenceStatus::kOk, tracker);
    EXPECT_EQ(third_result.historical_health, HistoricalHealthStatus::kError);
}

}  // namespace
}  // namespace score::mw::com::impl::e2e
