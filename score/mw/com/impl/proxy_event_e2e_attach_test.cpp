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
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/sample_ptr.h"
#include "score/mw/com/impl/proxy_event.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>

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

    /// \brief Makes the binding deliver one sample carrying the given outcome, as a binding running its E2E check
    /// would.
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

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleSummaryIsOkForValidBindingResult)
{
    // Given a binding that reports a valid data-integrity and sequence result for the delivered sample
    DeliverSampleWithBindingResult(e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kOk, e2e::SequenceStatus::kOk});

    // When receiving the sample
    const auto delivered_result = GetNewSampleResult();

    // Then the delivered SamplePtr carries the binding's pair and a binding-independent summary of kOk
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kOk);
    EXPECT_EQ(delivered_result.sequence, e2e::SequenceStatus::kOk);
    EXPECT_EQ(delivered_result.historical_health, e2e::HistoricalHealthStatus::kDisabled);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kOk);
}

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleSummaryIsOkForSequenceGapWithinThreshold)
{
    // Given a binding that reports a tolerated sequence gap
    DeliverSampleWithBindingResult(
        e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kOk, e2e::SequenceStatus::kOkGapWithinThreshold});

    // When receiving the sample
    const auto delivered_result = GetNewSampleResult();

    // Then the summary is still kOk
    EXPECT_EQ(delivered_result.sequence, e2e::SequenceStatus::kOkGapWithinThreshold);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kOk);
}

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleSummaryIsErrorForBindingDataIntegrityError)
{
    // Given a binding that reports a data-integrity error for the delivered sample
    DeliverSampleWithBindingResult(e2e::RawCheckOutcome{e2e::DataIntegrityStatus::kError, e2e::SequenceStatus::kOk});

    // When receiving the sample
    const auto delivered_result = GetNewSampleResult();

    // Then the summary is kError
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kError);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kError);
}

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleSummaryIsErrorForBindingSequenceError)
{
    // Given a binding that reports a repeated sequence counter for the delivered sample
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
    // Given a binding that delivers a sample without any E2E result (no E2E configured for the event)
    mock_proxy_event_.PushFakeSample(TestSampleType{4242U});

    // When receiving the sample
    const auto delivered_result = GetNewSampleResult();

    // Then the delivered SamplePtr reports the default, disabled result
    EXPECT_EQ(delivered_result.data_integrity, e2e::DataIntegrityStatus::kDisabled);
    EXPECT_EQ(delivered_result.sequence, e2e::SequenceStatus::kDisabled);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kDisabled);
}

}  // namespace
}  // namespace score::mw::com::impl
