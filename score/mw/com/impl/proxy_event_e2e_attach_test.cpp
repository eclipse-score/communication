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
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"

#include <gtest/gtest.h>

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
};

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleCarriesTheBindingsE2EResult)
{
    // Given a binding that reports a non-default E2E result for the next delivered sample
    const e2e::E2EResult expected_result{
        e2e::DataIntegrityStatus::kOk, e2e::SequenceStatus::kOk, e2e::HistoricalHealthStatus::kDisabled,
        e2e::Summary::kOk};
    ON_CALL(mock_proxy_event_, GetLastE2EResult()).WillByDefault(::testing::Return(expected_result));
    mock_proxy_event_.PushFakeSample(TestSampleType{4242U});
    std::ignore = proxy_event_.Subscribe(1U);

    // When receiving the sample
    e2e::E2EResult delivered_result{};
    const auto result = proxy_event_.GetNewSamples(
        [&delivered_result](SamplePtr<TestSampleType> sample) { delivered_result = sample.GetE2EResult(); }, 1U);

    // Then the delivered SamplePtr carries the binding's E2E result
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1U);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kOk);
}

TEST_F(ProxyEventE2EAttachFixture, DeliveredSampleDefaultsToDisabledWhenBindingHasNoE2EResult)
{
    // Given a binding that never overrides GetLastE2EResult() (default, disabled)
    mock_proxy_event_.PushFakeSample(TestSampleType{4242U});
    std::ignore = proxy_event_.Subscribe(1U);

    // When receiving the sample
    e2e::E2EResult delivered_result{e2e::DataIntegrityStatus::kOk,
                                    e2e::SequenceStatus::kOk,
                                    e2e::HistoricalHealthStatus::kOk,
                                    e2e::Summary::kOk};
    const auto result = proxy_event_.GetNewSamples(
        [&delivered_result](SamplePtr<TestSampleType> sample) { delivered_result = sample.GetE2EResult(); }, 1U);

    // Then the delivered SamplePtr reports the default, disabled result
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1U);
    EXPECT_EQ(delivered_result.summary, e2e::Summary::kDisabled);
}

}  // namespace
}  // namespace score::mw::com::impl
