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
                                                           /* recovery_threshold */ 1U};

constexpr HealthTrackerConfiguration kDisabledConfiguration{/* enabled */ false,
                                                            /* error_threshold */ 3U,
                                                            /* recovery_threshold */ 1U};

// --- ComputeSummary --------------------------------------------------------

TEST(E2eSummaryTest, AllDisabledReportsSummaryDisabled)
{
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kDisabled, SequenceStatus::kDisabled, HistoricalHealthStatus::kDisabled);

    EXPECT_EQ(summary, Summary::kDisabled);
}

TEST(E2eSummaryTest, AllEnabledAllPassReportsOk)
{
    const auto summary = ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kOk, HistoricalHealthStatus::kOk);

    EXPECT_EQ(summary, Summary::kOk);
}

TEST(E2eSummaryTest, AllEnabledWithGapWithinThresholdStillReportsOk)
{
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kOkGapWithinThreshold, HistoricalHealthStatus::kOk);

    EXPECT_EQ(summary, Summary::kOk);
}

TEST(E2eSummaryTest, HistoricalHealthDisabledOthersPassReportsOkWithDisabledChecks)
{
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kOk, HistoricalHealthStatus::kDisabled);

    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, SequenceDisabledOthersPassReportsOkWithDisabledChecks)
{
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kDisabled, HistoricalHealthStatus::kOk);

    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, DataIntegrityDisabledOthersPassReportsOkWithDisabledChecks)
{
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kDisabled, SequenceStatus::kOk, HistoricalHealthStatus::kOk);

    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, OnlyDataIntegrityEnabledReportsOkWithDisabledChecks)
{
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kDisabled, HistoricalHealthStatus::kDisabled);

    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, OnlySequenceEnabledReportsOkWithDisabledChecks)
{
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kDisabled, SequenceStatus::kOk, HistoricalHealthStatus::kDisabled);

    EXPECT_EQ(summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, DataIntegrityErrorReportsError)
{
    const auto summary = ComputeSummary(DataIntegrityStatus::kError, SequenceStatus::kOk, HistoricalHealthStatus::kOk);

    EXPECT_EQ(summary, Summary::kError);
}

TEST(E2eSummaryTest, SequenceRepeatedErrorReportsError)
{
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kErrorRepeated, HistoricalHealthStatus::kOk);

    EXPECT_EQ(summary, Summary::kError);
}

TEST(E2eSummaryTest, SequenceGapExceedsThresholdErrorReportsError)
{
    const auto summary = ComputeSummary(
        DataIntegrityStatus::kOk, SequenceStatus::kErrorGapExceedsThreshold, HistoricalHealthStatus::kOk);

    EXPECT_EQ(summary, Summary::kError);
}

TEST(E2eSummaryTest, HistoricalHealthErrorReportsError)
{
    const auto summary = ComputeSummary(DataIntegrityStatus::kOk, SequenceStatus::kOk, HistoricalHealthStatus::kError);

    EXPECT_EQ(summary, Summary::kError);
}

// Doc's misconfiguration Case 5: HistoricalHealthTrackingEnabled=true while both underlying checks
// are disabled forces historical_health to kError even though data_integrity/sequence are
// kDisabled. Error must take priority over the "all disabled" shortcut, so this must be kError,
// not kDisabled.
TEST(E2eSummaryTest, HistoricalHealthErrorWithOtherFieldsDisabledReportsErrorNotDisabled)
{
    const auto summary =
        ComputeSummary(DataIntegrityStatus::kDisabled, SequenceStatus::kDisabled, HistoricalHealthStatus::kError);

    EXPECT_EQ(summary, Summary::kError);
}

// --- BuildE2EResult ---------------------------------------------------------

TEST(E2eSummaryTest, BuildE2EResultPopulatesAllFourFieldsConsistently)
{
    HealthContext context{};

    const auto result = BuildE2EResult(DataIntegrityStatus::kOk, SequenceStatus::kOk, kEnabledConfiguration, context);

    EXPECT_EQ(result.data_integrity, DataIntegrityStatus::kOk);
    EXPECT_EQ(result.sequence, SequenceStatus::kOk);
    EXPECT_EQ(result.historical_health, HistoricalHealthStatus::kOk);
    EXPECT_EQ(result.summary, Summary::kOk);
}

TEST(E2eSummaryTest, BuildE2EResultWithDisabledHealthTrackerStillComputesSummary)
{
    HealthContext context{};

    const auto result = BuildE2EResult(DataIntegrityStatus::kOk, SequenceStatus::kOk, kDisabledConfiguration, context);

    EXPECT_EQ(result.historical_health, HistoricalHealthStatus::kDisabled);
    EXPECT_EQ(result.summary, Summary::kOkWithDisabledChecks);
}

TEST(E2eSummaryTest, BuildE2EResultWithFailingSampleReportsErrorAndAdvancesHealthContext)
{
    HealthContext context{};

    const auto result =
        BuildE2EResult(DataIntegrityStatus::kError, SequenceStatus::kOk, kEnabledConfiguration, context);

    EXPECT_EQ(result.summary, Summary::kError);
    EXPECT_EQ(context.error_counter, 1U);
}

}  // namespace
}  // namespace score::mw::com::impl::e2e
