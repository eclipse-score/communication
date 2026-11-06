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
#include "score/mw/com/impl/bindings/mock_binding/generic_proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/sample_ptr.h"
#include "score/mw/com/impl/generic_proxy_event.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/test/proxy_resources.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

using TestSampleType = std::uint16_t;
constexpr auto kEventName{"DummyE2EEvent"};

class ProxyEventE2EAttachFixture : public ::testing::Test
{
  protected:
    ProxyEventE2EAttachFixture()
        : mock_proxy_event_ptr_{std::make_unique<::testing::NiceMock<mock_binding::ProxyEvent>>()},
          mock_proxy_event_{*mock_proxy_event_ptr_},
          proxy_event_{kEventName, std::move(mock_proxy_event_ptr_)}
    {
        ON_CALL(mock_proxy_event_, GetSubscriptionState())
            .WillByDefault(::testing::Return(SubscriptionState::kNotSubscribed));
        ON_CALL(mock_proxy_event_, Subscribe(::testing::_)).WillByDefault(::testing::Return(Result<void>{}));
    }

    std::unique_ptr<::testing::NiceMock<mock_binding::ProxyEvent>> mock_proxy_event_ptr_;
    ::testing::NiceMock<mock_binding::ProxyEvent>& mock_proxy_event_;
    ProxyEvent<TestSampleType> proxy_event_;

    /// \brief Makes the binding deliver one sample with the given outcome.
    void DeliverSampleWithBindingResult(const e2e::RawCheckOutcome& binding_outcome)
    {
        ON_CALL(mock_proxy_event_, GetNewSamples(::testing::_, ::testing::_))
            .WillByDefault([binding_outcome](ProxyEventBinding::Callback&& receiver,
                                             TrackerGuardFactory& tracker) noexcept -> Result<std::size_t> {
                SamplePtr<void> sample{mock_binding::MakeSamplePtr(new TestSampleType{4242U}),
                                       std::move(*tracker.TakeGuard()),
                                       binding_outcome};
                receiver(std::move(sample), tracing::ITracingRuntime::TracePointDataId{0U});
                return Result<std::size_t>{1U};
            });
    }

    e2e::E2EResult GetNewSampleResult()
    {
        std::ignore = proxy_event_.Subscribe(1U);
        e2e::E2EResult delivered_result{};
        const auto result = proxy_event_.GetNewSamples(
            [&delivered_result](SamplePtr<TestSampleType> sample) {
                delivered_result = sample.GetE2EResult();
            },
            1U);
        EXPECT_TRUE(result.has_value());
        return delivered_result;
    }
};

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleSummaryIsOkWithDisabledChecksForValidBindingResult)
{
    // Given a valid binding result
    DeliverSampleWithBindingResult(e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kOk, e2e::SequenceStatus::kOk});

    // When receiving the sample
    const auto delivered_result = GetNewSampleResult();

    // Then the summary is kOkWithDisabledChecks, as historical health is disabled
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kOk);
    EXPECT_EQ(delivered_result.sequence, e2e::SequenceStatus::kOk);
    EXPECT_EQ(delivered_result.historical_health, e2e::HistoricalHealthStatus::kDisabled);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kOkWithDisabledChecks);
}

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleSummaryIsOkWithDisabledChecksForSequenceGapWithinThreshold)
{
    // Given a sequence gap within the threshold
    DeliverSampleWithBindingResult(
        e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kOk, e2e::SequenceStatus::kOkGapWithinThreshold});

    // When receiving the sample
    const auto delivered_result = GetNewSampleResult();

    // Then the gap status is kept and the summary is kOkWithDisabledChecks
    EXPECT_EQ(delivered_result.sequence, e2e::SequenceStatus::kOkGapWithinThreshold);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kOkWithDisabledChecks);
}

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleSummaryIsErrorForBindingDataIntegrityError)
{
    // Given a data-integrity error
    DeliverSampleWithBindingResult(e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kError, e2e::SequenceStatus::kOk});

    // When receiving the sample
    const auto delivered_result = GetNewSampleResult();

    // Then the summary is kError
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kError);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kError);
}

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleSummaryIsErrorForBindingSequenceError)
{
    // Given a repeated sequence counter
    DeliverSampleWithBindingResult(
        e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kOk, e2e::SequenceStatus::kErrorRepeated});

    // When receiving the sample
    const auto delivered_result = GetNewSampleResult();

    // Then the summary is kError
    EXPECT_EQ(delivered_result.sequence, e2e::SequenceStatus::kErrorRepeated);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kError);
}

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleStaysDisabledWhenBindingDidNotCheck)
{
    // Given a sample without E2E result
    mock_proxy_event_.PushFakeSample(TestSampleType{4242U});

    // When receiving the sample
    const auto delivered_result = GetNewSampleResult();

    // Then the result is kDisabled
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kDisabled);
    EXPECT_EQ(delivered_result.sequence, e2e::SequenceStatus::kDisabled);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kDisabled);
}

/// \brief Health tracking enabled: 2 failures in a window of 3 give kError, 2 passes give kOk again.
class ProxyEventE2EHealthFixture : public ProxyEventE2EAttachFixture
{
  protected:
    ProxyEventE2EHealthFixture()
    {
        e2e::HealthTrackerConfiguration configuration{};
        configuration.enabled = true;
        configuration.error_threshold = 2U;
        configuration.recovery_threshold = 2U;
        configuration.window_size = 3U;
        ProxyEventBaseAttorney{proxy_event_}.SetHealthTrackerConfiguration(configuration);
    }

    static constexpr std::size_t kMaxSamples{4U};

    static constexpr e2e::RawCheckOutcome kPassed{e2e::DataIntegrityStatus::kOk, e2e::SequenceStatus::kOk};
    static constexpr e2e::RawCheckOutcome kFailed{e2e::DataIntegrityStatus::kError, e2e::SequenceStatus::kOk};

    /// \brief Makes the binding deliver one sample per outcome.
    void DeliverSamplesWithBindingResults(std::vector<e2e::RawCheckOutcome> binding_outcomes)
    {
        ON_CALL(mock_proxy_event_, GetNewSamples(::testing::_, ::testing::_))
            .WillByDefault([binding_outcomes = std::move(binding_outcomes)](
                               ProxyEventBinding::Callback&& receiver,
                               TrackerGuardFactory& tracker) noexcept -> Result<std::size_t> {
                for (const auto& binding_outcome : binding_outcomes)
                {
                    SamplePtr<void> sample{mock_binding::MakeSamplePtr(new TestSampleType{4242U}),
                                           std::move(*tracker.TakeGuard()),
                                           binding_outcome};
                    receiver(std::move(sample), tracing::ITracingRuntime::TracePointDataId{0U});
                }
                return Result<std::size_t>{binding_outcomes.size()};
            });
    }

    /// \brief Returns the E2EResult of every delivered sample.
    std::vector<e2e::E2EResult> GetNewSampleResults()
    {
        std::vector<e2e::E2EResult> delivered_results{};
        const auto result = proxy_event_.GetNewSamples(
            [&delivered_results](SamplePtr<TestSampleType> sample) {
                delivered_results.push_back(sample.GetE2EResult());
            },
            kMaxSamples);
        EXPECT_TRUE(result.has_value());
        return delivered_results;
    }
};

TEST_F(ProxyEventE2EHealthFixture, HistoricalHealthStaysOkBelowErrorThreshold)
{
    // Given one failed sample
    std::ignore = proxy_event_.Subscribe(kMaxSamples);
    DeliverSamplesWithBindingResults({kFailed});

    // When receiving the sample
    const auto delivered_results = GetNewSampleResults();

    // Then historical health is kOk, but the summary is kError
    ASSERT_EQ(delivered_results.size(), 1U);
    EXPECT_EQ(delivered_results[0].historical_health, e2e::HistoricalHealthStatus::kOk);
    EXPECT_EQ(delivered_results[0].summary, e2e::Summary::kError);
}

TEST_F(ProxyEventE2EHealthFixture, HistoricalHealthTurnsErrorWhenErrorThresholdIsReached)
{
    // Given two failed samples
    std::ignore = proxy_event_.Subscribe(kMaxSamples);
    DeliverSamplesWithBindingResults({kFailed, kFailed});

    // When receiving the samples
    const auto delivered_results = GetNewSampleResults();

    // Then historical health turns kError with the second sample
    ASSERT_EQ(delivered_results.size(), 2U);
    EXPECT_EQ(delivered_results[0].historical_health, e2e::HistoricalHealthStatus::kOk);
    EXPECT_EQ(delivered_results[1].historical_health, e2e::HistoricalHealthStatus::kError);
}

TEST_F(ProxyEventE2EHealthFixture, HistoricalHealthRecoversWhenRecoveryThresholdIsReached)
{
    // Given two failed and then two passed samples
    std::ignore = proxy_event_.Subscribe(kMaxSamples);
    DeliverSamplesWithBindingResults({kFailed, kFailed, kPassed, kPassed});

    // When receiving the samples
    const auto delivered_results = GetNewSampleResults();

    // Then historical health recovers to kOk with the second passed sample
    ASSERT_EQ(delivered_results.size(), 4U);
    EXPECT_EQ(delivered_results[2].historical_health, e2e::HistoricalHealthStatus::kError);
    EXPECT_EQ(delivered_results[2].summary, e2e::Summary::kError);
    EXPECT_EQ(delivered_results[3].historical_health, e2e::HistoricalHealthStatus::kOk);
    EXPECT_EQ(delivered_results[3].summary, e2e::Summary::kOk);
}

TEST_F(ProxyEventE2EHealthFixture, SampleWithoutExecutedChecksDoesNotChangeHistoricalHealth)
{
    // Given a sample on which no check was executed
    std::ignore = proxy_event_.Subscribe(kMaxSamples);
    DeliverSamplesWithBindingResults({e2e::RawCheckOutcome{}});

    // When receiving the sample
    const auto delivered_results = GetNewSampleResults();

    // Then historical health and summary are kDisabled
    ASSERT_EQ(delivered_results.size(), 1U);
    EXPECT_EQ(delivered_results[0].historical_health, e2e::HistoricalHealthStatus::kDisabled);
    EXPECT_EQ(delivered_results[0].summary, e2e::Summary::kDisabled);
}

TEST_F(ProxyEventE2EHealthFixture, HistoricalHealthIsResetByResubscribe)
{
    // Given historical health already at kError
    std::ignore = proxy_event_.Subscribe(kMaxSamples);
    DeliverSamplesWithBindingResults({kFailed, kFailed});
    const auto results_before_resubscribe = GetNewSampleResults();
    ASSERT_EQ(results_before_resubscribe.size(), 2U);
    ASSERT_EQ(results_before_resubscribe[1].historical_health, e2e::HistoricalHealthStatus::kError);

    // When the user unsubscribes, subscribes again and receives a passed sample
    proxy_event_.Unsubscribe();
    std::ignore = proxy_event_.Subscribe(kMaxSamples);
    DeliverSamplesWithBindingResults({kPassed});
    const auto results_after_resubscribe = GetNewSampleResults();

    // Then the history is reset and historical health is kOk
    ASSERT_EQ(results_after_resubscribe.size(), 1U);
    EXPECT_EQ(results_after_resubscribe[0].historical_health, e2e::HistoricalHealthStatus::kOk);
    EXPECT_EQ(results_after_resubscribe[0].summary, e2e::Summary::kOk);
}

TEST_F(ProxyEventE2EHealthFixture, HistoricalHealthIsKeptWithoutUserResubscribe)
{
    // Given historical health already at kError
    std::ignore = proxy_event_.Subscribe(kMaxSamples);
    DeliverSamplesWithBindingResults({kFailed, kFailed});
    const auto results_before = GetNewSampleResults();
    ASSERT_EQ(results_before.size(), 2U);

    // When a passed sample arrives without the user resubscribing
    DeliverSamplesWithBindingResults({kPassed});
    const auto results_after = GetNewSampleResults();

    // Then the history is kept and historical health stays kError
    ASSERT_EQ(results_after.size(), 1U);
    EXPECT_EQ(results_after[0].historical_health, e2e::HistoricalHealthStatus::kError);
}

TEST(ProxyEventE2EConfigurationTest, HealthTrackerConfigurationIsTakenFromBinding)
{
    // Given a binding with an enabled health tracker configuration
    auto mock_proxy_event_ptr = std::make_unique<::testing::NiceMock<mock_binding::ProxyEvent>>();
    auto& mock_proxy_event = *mock_proxy_event_ptr;
    e2e::HealthTrackerConfiguration configuration{};
    configuration.enabled = true;
    configuration.error_threshold = 2U;
    configuration.recovery_threshold = 2U;
    configuration.window_size = 3U;
    mock_proxy_event.SetHealthTrackerConfiguration(configuration);
    ON_CALL(mock_proxy_event, GetSubscriptionState())
        .WillByDefault(::testing::Return(SubscriptionState::kNotSubscribed));
    ON_CALL(mock_proxy_event, Subscribe(::testing::_)).WillByDefault(::testing::Return(Result<void>{}));
    ON_CALL(mock_proxy_event, GetNewSamples(::testing::_, ::testing::_))
        .WillByDefault(
            [](ProxyEventBinding::Callback&& receiver, TrackerGuardFactory& tracker) noexcept -> Result<std::size_t> {
                for (std::size_t i = 0U; i < 2U; ++i)
                {
                    SamplePtr<void> sample{
                        mock_binding::MakeSamplePtr(new TestSampleType{4242U}),
                        std::move(*tracker.TakeGuard()),
                        e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kError, e2e::SequenceStatus::kOk}};
                    receiver(std::move(sample), tracing::ITracingRuntime::TracePointDataId{0U});
                }
                return Result<std::size_t>{2U};
            });
    ProxyEvent<TestSampleType> proxy_event{kEventName, std::move(mock_proxy_event_ptr)};
    std::ignore = proxy_event.Subscribe(2U);

    // When receiving two failed samples
    std::vector<e2e::E2EResult> delivered_results{};
    const auto result = proxy_event.GetNewSamples(
        [&delivered_results](SamplePtr<TestSampleType> sample) {
            delivered_results.push_back(sample.GetE2EResult());
        },
        2U);

    // Then historical health is tracked without further configuration
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(delivered_results.size(), 2U);
    EXPECT_EQ(delivered_results[0].historical_health, e2e::HistoricalHealthStatus::kOk);
    EXPECT_EQ(delivered_results[1].historical_health, e2e::HistoricalHealthStatus::kError);
}

class GenericProxyEventE2EAttachFixture : public ::testing::Test
{
  protected:
    GenericProxyEventE2EAttachFixture()
        : mock_proxy_event_ptr_{std::make_unique<::testing::NiceMock<mock_binding::GenericProxyEvent>>()},
          mock_proxy_event_{*mock_proxy_event_ptr_},
          proxy_event_{kEventName, std::unique_ptr<GenericProxyEventBinding>{std::move(mock_proxy_event_ptr_)}}
    {
        ON_CALL(mock_proxy_event_, GetSubscriptionState())
            .WillByDefault(::testing::Return(SubscriptionState::kNotSubscribed));
        ON_CALL(mock_proxy_event_, Subscribe(::testing::_)).WillByDefault(::testing::Return(Result<void>{}));
    }

    void DeliverSamplesWithBindingResults(std::vector<e2e::RawCheckOutcome> binding_outcomes)
    {
        ON_CALL(mock_proxy_event_, GetNewSamples(::testing::_, ::testing::_))
            .WillByDefault([binding_outcomes = std::move(binding_outcomes)](
                               ProxyEventBinding::Callback&& receiver,
                               TrackerGuardFactory& tracker) noexcept -> Result<std::size_t> {
                for (const auto& binding_outcome : binding_outcomes)
                {
                    SamplePtr<void> sample{mock_binding::MakeSamplePtr(new TestSampleType{4242U}),
                                           std::move(*tracker.TakeGuard()),
                                           binding_outcome};
                    receiver(std::move(sample), tracing::ITracingRuntime::TracePointDataId{0U});
                }
                return Result<std::size_t>{binding_outcomes.size()};
            });
    }

    std::vector<e2e::E2EResult> GetNewSampleResults(const std::size_t max_samples)
    {
        std::ignore = proxy_event_.Subscribe(max_samples);
        std::vector<e2e::E2EResult> delivered_results{};
        const auto result = proxy_event_.GetNewSamples(
            [&delivered_results](SamplePtr<void> sample) {
                delivered_results.push_back(sample.GetE2EResult());
            },
            max_samples);
        EXPECT_TRUE(result.has_value());
        return delivered_results;
    }

    std::unique_ptr<::testing::NiceMock<mock_binding::GenericProxyEvent>> mock_proxy_event_ptr_;
    ::testing::NiceMock<mock_binding::GenericProxyEvent>& mock_proxy_event_;
    GenericProxyEvent proxy_event_;

    void DeliverSampleWithBindingResult(const e2e::RawCheckOutcome& binding_outcome)
    {
        ON_CALL(mock_proxy_event_, GetNewSamples(::testing::_, ::testing::_))
            .WillByDefault([binding_outcome](GenericProxyEventBinding::Callback&& receiver,
                                             TrackerGuardFactory& tracker) noexcept -> Result<std::size_t> {
                SamplePtr<void> sample{mock_binding::MakeSamplePtr(new TestSampleType{4242U}),
                                       std::move(*tracker.TakeGuard()),
                                       binding_outcome};
                receiver(std::move(sample), tracing::ITracingRuntime::TracePointDataId{0U});
                return Result<std::size_t>{1U};
            });
    }

    e2e::E2EResult GetNewSampleResult()
    {
        std::ignore = proxy_event_.Subscribe(1U);
        e2e::E2EResult delivered_result{};
        const auto result = proxy_event_.GetNewSamples(
            [&delivered_result](SamplePtr<void> sample) noexcept {
                delivered_result = sample.GetE2EResult();
            },
            1U);
        EXPECT_TRUE(result.has_value());
        return delivered_result;
    }
};

TEST_F(GenericProxyEventE2EAttachFixture, DeliveredSampleSummaryIsOkWithDisabledChecksForValidBindingResult)
{
    // Given a binding that reports a valid data-integrity and sequence result for the delivered generic sample
    DeliverSampleWithBindingResult(e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kOk, e2e::SequenceStatus::kOk});

    // When receiving the sample through the generic proxy event
    const auto delivered_result = GetNewSampleResult();

    // Then the summary is completed the same way as for a typed proxy event
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kOk);
    EXPECT_EQ(delivered_result.sequence, e2e::SequenceStatus::kOk);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kOkWithDisabledChecks);
}

TEST_F(GenericProxyEventE2EAttachFixture, DeliveredSampleSummaryIsErrorForBindingDataIntegrityError)
{
    // Given a binding that reports a data-integrity error for the delivered generic sample
    DeliverSampleWithBindingResult(e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kError, e2e::SequenceStatus::kOk});

    // When receiving the sample through the generic proxy event
    const auto delivered_result = GetNewSampleResult();

    // Then the summary is kError
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kError);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kError);
}

TEST_F(GenericProxyEventE2EAttachFixture, HistoricalHealthIsTrackedWhenEnabled)
{
    // Given an enabled health tracker and two failed samples
    e2e::HealthTrackerConfiguration configuration{};
    configuration.enabled = true;
    configuration.error_threshold = 2U;
    configuration.recovery_threshold = 2U;
    configuration.window_size = 3U;
    ProxyEventBaseAttorney{proxy_event_}.SetHealthTrackerConfiguration(configuration);
    const e2e::RawCheckOutcome failed{e2e::DataIntegrityStatus::kError, e2e::SequenceStatus::kOk};
    DeliverSamplesWithBindingResults({failed, failed});

    // When receiving the samples through the generic event
    const auto delivered_results = GetNewSampleResults(2U);

    // Then historical health turns kError with the second sample
    ASSERT_EQ(delivered_results.size(), 2U);
    EXPECT_EQ(delivered_results[0].historical_health, e2e::HistoricalHealthStatus::kOk);
    EXPECT_EQ(delivered_results[1].historical_health, e2e::HistoricalHealthStatus::kError);
}

}  // namespace
}  // namespace score::mw::com::impl
