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

#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_fixture.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/sample_reference_tracker.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using LolaProxyEventGetNewSamplesFixture = LolaProxyEventFixture;
using LolaProxyEventGetNumNewSamplesAvailableFixture = LolaProxyEventFixture;

TEST_F(LolaProxyEventGetNewSamplesFixture, CallsReceiverForEachAccessibleSample)
{
    this->RecordProperty("Verifies", "SCR-14035773, SCR-21350367, SCR-6225206");
    this->RecordProperty(
        "Description",
        "Checks that GetNewSamples will get new samples from provider. Slot referencing works (req. SCR-6225206)");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent containing two samples with a max sample count larger
    // than number of samples available
    const std::size_t max_sample_count_subscription{5U};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(max_sample_count_subscription)
        .WithSkeletonEventData(
            {{kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}});

    // When calling GetNewSamples with a max_samples higher than the number of samples available
    const std::size_t max_samples{5U};
    std::uint16_t num_callbacks_called{0U};
    CallbackCountingReceiver callback_counting_receiver{num_callbacks_called};
    const auto num_callbacks_result = this->GetNewSamples(callback_counting_receiver, max_samples);

    // Then the returned value will be equal to the number of times the callback was called which is once per
    // SkeletonEvent sample
    ASSERT_TRUE(num_callbacks_result.has_value());
    ASSERT_EQ(num_callbacks_result.value(), 2U);
    ASSERT_EQ(num_callbacks_called, 2U);
}

TEST_F(LolaProxyEventGetNewSamplesFixture, CallsReceiverForEachAccessibleSampleLimitedBySubscription)
{
    this->RecordProperty("Verifies", "SCR-14035773, SCR-21350367, SCR-6225206");
    this->RecordProperty(
        "Description",
        "Checks that GetNewSamples will get new samples from provider. Slot referencing works (req. SCR-6225206)");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent containing two samples with a max sample count smaller
    // than number of samples available
    const std::size_t max_sample_count_subscription{1U};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(max_sample_count_subscription)
        .WithSkeletonEventData(
            {{kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}});

    // When calling GetNewSamples with a max_samples higher than the number of samples available
    const std::size_t max_samples{5U};
    std::uint16_t num_callbacks_called{0U};
    CallbackCountingReceiver callback_counting_receiver{num_callbacks_called};
    const auto num_callbacks_result = this->GetNewSamples(callback_counting_receiver, max_samples);

    // Then the returned value will be equal to the number of times the callback was called which is once per
    // SkeletonEvent sample (limited by the max sample count set in subscription)
    ASSERT_TRUE(num_callbacks_result.has_value());
    ASSERT_EQ(num_callbacks_result.value(), 1U);
    ASSERT_EQ(num_callbacks_called, 1U);
}

TEST_F(LolaProxyEventGetNewSamplesFixture, CallsReceiverForEachAccessibleSampleLimitedByMaxSampleCount)
{
    this->RecordProperty("Verifies", "SCR-14035773, SCR-21350367, SCR-6225206");
    this->RecordProperty(
        "Description",
        "Checks that GetNewSamples will get new samples from provider. Slot referencing works (req. SCR-6225206)");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent containing two samples with a max sample count larger
    // than number of samples available
    const std::size_t max_sample_count_subscription{5U};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(max_sample_count_subscription)
        .WithSkeletonEventData(
            {{kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}});

    // When calling GetNewSamples with a max_samples smaller than the number of samples available
    const std::size_t max_samples{1U};
    std::uint16_t num_callbacks_called{0U};
    CallbackCountingReceiver callback_counting_receiver{num_callbacks_called};
    const auto num_callbacks_result = this->GetNewSamples(callback_counting_receiver, max_samples);

    // Then the returned value will be equal to the number of times the callback was called which is once per
    // SkeletonEvent sample (limited by the max sample count set in GetNewSamples)
    ASSERT_TRUE(num_callbacks_result.has_value());
    ASSERT_EQ(num_callbacks_result.value(), 1U);
    ASSERT_EQ(num_callbacks_called, 1U);
}

TEST_F(LolaProxyEventGetNewSamplesFixture, CallsReceiverForEachAccessibleSampleLimitedByCurrentlyHeldSamples)
{
    this->RecordProperty("Verifies", "SCR-14035773, SCR-21350367, SCR-6225206");
    this->RecordProperty(
        "Description",
        "Checks that GetNewSamples will get new samples from provider. Slot referencing works (req. SCR-6225206)");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent containing two samples with a max sample count of 2
    const std::size_t max_sample_count_subscription{2U};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(max_sample_count_subscription)
        .WithSkeletonEventData(
            {{kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}});

    // and given that GetNewSamples is called and one SamplePtr is saved
    const std::size_t max_samples{1U};
    impl::SamplePtr<void> saved_sample_ptr{};
    score::cpp::ignore = this->GetNewSamples(
        [&saved_sample_ptr](impl::SamplePtr<void> sample_ptr, const tracing::ITracingRuntime::TracePointDataId) {
            saved_sample_ptr = std::move(sample_ptr);
        },
        max_samples);

    // and an additional 2 samples are provided by the SkeletonEvent
    this->PutData(kDummySampleValue + 2U, kDummyInputTimestamp + 2U);
    this->PutData(kDummySampleValue + 3U, kDummyInputTimestamp + 3U);

    // When calling GetNewSamples with a max_samples of 2
    const std::size_t max_samples_2{2U};
    std::uint16_t num_callbacks_called{0U};
    CallbackCountingReceiver callback_counting_receiver{num_callbacks_called};
    const auto num_callbacks_result = this->GetNewSamples(callback_counting_receiver, max_samples_2);

    // Then the returned value will be equal to the number of times the callback was called which is only once since the
    // max sample count is 2 and a sample is already reserved since we're storing one SamplePtr
    ASSERT_TRUE(num_callbacks_result.has_value());
    ASSERT_EQ(num_callbacks_result.value(), 1U);
    ASSERT_EQ(num_callbacks_called, 1U);
}

TEST_F(LolaProxyEventGetNewSamplesFixture, CallsReceiverWithDataFromProviderInCorrectOrder)
{
    this->RecordProperty("Verifies", "SCR-14035773, SCR-21350367");
    this->RecordProperty("Description", "Checks that GetNewSamples will get new samples from provider.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent containing two samples with a max sample count larger
    // than number of samples available
    std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> values_to_send{
        {kDummySampleValue, kDummyInputTimestamp},
        {kDummySampleValue + 1U, kDummyInputTimestamp + 1U},
        {kDummySampleValue + 2U, kDummyInputTimestamp + 2U}};
    const std::size_t max_sample_count_subscription{5U};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(max_sample_count_subscription)
        .WithSkeletonEventData(values_to_send);

    // When calling GetNewSamples with a max_samples higher than the number of samples available
    const std::size_t max_samples{5U};
    std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> received_samples{};
    score::cpp::ignore = this->GetNewSamples(
        [&received_samples](impl::SamplePtr<void> sample, const tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);

            const auto value = GetSamplePtrValue(sample.get());
            received_samples.emplace_back(value, timestamp);
        },
        max_samples);

    // Then the data provided to the receiver will be the same data provided by the SkeletonEvent and in the same order
    EXPECT_EQ(values_to_send, received_samples);
}

TEST_F(LolaProxyEventGetNewSamplesFixture, DoNotReceiveEventsFromThePast)
{
    this->RecordProperty("Verifies", "SCR-14035773, SCR-21350367");
    this->RecordProperty("Description",
                         "Sends multiple events and checks that reported number of new samples is correct and no "
                         "samples of the past are reported/received.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent containing one sample
    constexpr EventSlotStatus::EventTimeStamp input_timestamp{17U};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(2U)
        .WithSkeletonEventData({{kDummySampleValue, input_timestamp}});

    // and given that GetNewSamples was called once
    const std::size_t max_samples{37U};
    score::cpp::ignore = this->GetNewSamples([](auto, auto) noexcept {}, max_samples);

    // and new data is provided by the SkeletonEvent which is older than the previous data
    constexpr EventSlotStatus::EventTimeStamp input_timestamp_2{input_timestamp - 1U};
    constexpr TestSampleType input_value_2{kDummySampleValue + 1U};
    this->PutData(input_value_2, input_timestamp_2);

    // When calling GetNewSamples
    const auto new_num_samples = this->GetNewSamples(
        [](auto, auto) {
            FAIL() << "Callback was called although no sample was expected.";
        },
        max_samples);

    // Then no data should be received and the receiver should never be called
    ASSERT_TRUE(new_num_samples.has_value());
    EXPECT_EQ(new_num_samples.value(), 0);
}

TEST_F(LolaProxyEventGetNewSamplesFixture, TransmitEventInShmArea)
{
    this->RecordProperty("Verifies", "SCR-6367235");
    this->RecordProperty("Description", "A valid SamplePtr shall reference a valid and correct slot.");
    this->RecordProperty("TestType ", "Requirements-based test");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_).ThatIsSubscribedWithMaxSamples(1U);

    // and given that the SkeletonEvent contains one sample in a given slot
    const EventSlotStatus::EventTimeStamp input_timestamp{1U};
    const auto slot_index = this->PutData(kDummySampleValue, input_timestamp);

    // When calling GetNewSamples
    const std::size_t max_samples{1U};
    score::cpp::ignore = this->GetNewSamples(
        [this, slot_index](impl::SamplePtr<void>, const tracing::ITracingRuntime::TracePointDataId timestamp) {
            // Then the retrieved data is pointing to the same valid slot
            const auto& slot = (this->consumer_event_data_control_local_.value())[slot_index];
            EXPECT_FALSE(slot.IsInvalid());
            EXPECT_EQ(slot.GetTimeStamp(), timestamp);
        },
        max_samples);
}

TEST_F(LolaProxyEventGetNewSamplesFixture, ReturnsErrorWhenNotSubscribed)
{
    // Given a ProxyEvent that has not subscribed to a SkeletonEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .WithSkeletonEventData({{kDummySampleValue, kDummyInputTimestamp}});

    // When calling GetNewSamples
    const std::size_t max_samples{1U};
    SampleReferenceTracker sample_reference_tracker{};
    TrackerGuardFactory guard_factory{sample_reference_tracker.Allocate(max_samples)};
    const auto num_samples_result = this->test_proxy_event_->GetNewSamples(
        [](impl::SamplePtr<void>, auto) {
            FAIL() << "Callback called despite not having a valid subscription to the event.";
        },
        guard_factory);

    // Then an error is returned
    EXPECT_FALSE(num_samples_result.has_value());
}

TEST_F(LolaProxyEventGetNewSamplesFixture, CallsReceiverForEachAccessibleSampleWhenInSubscriptionPending)
{
    // Given a ProxyEvent that is in SubscriptionPending state
    const std::size_t max_sample_count_subscription{5U};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsInSubscriptionPendingWithMaxSamples(max_sample_count_subscription)
        .WithSkeletonEventData(
            {{kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}});

    // When calling GetNewSamples with a max_samples higher than the number of samples available
    const std::size_t max_samples{5U};
    std::uint16_t num_callbacks_called{0U};
    CallbackCountingReceiver callback_counting_receiver{num_callbacks_called};
    const auto num_callbacks_result = this->GetNewSamples(callback_counting_receiver, max_samples);

    // Then the returned value will be equal to the number of times the callback was called which is once per
    // SkeletonEvent sample
    ASSERT_TRUE(num_callbacks_result.has_value());
    ASSERT_EQ(num_callbacks_result.value(), 2U);
    ASSERT_EQ(num_callbacks_called, 2U);
}

TEST_F(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsNumberOfAvailableSamples)
{
    this->RecordProperty("Verifies", "SCR-21294278");
    this->RecordProperty("Description",
                         "Checks that GetNumNewSamplesAvailable reflects the number of new samples available.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent containing two samples
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(5U)
        .WithSkeletonEventData(
            {{kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}});

    // When calling GetNumNewSamplesAvailable
    const auto num_new_samples_available_result = this->test_proxy_event_->GetNumNewSamplesAvailable();

    // Then the returned value will be equal to the number of SkeletonEvent samples
    ASSERT_TRUE(num_new_samples_available_result.has_value());
    ASSERT_EQ(num_new_samples_available_result.value(), 2U);
}

TEST_F(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsNumberOfAvailableSamplesSinceLastGetNewSamples)
{
    this->RecordProperty("Verifies", "SCR-21294278");
    this->RecordProperty("Description",
                         "Checks that GetNumNewSamplesAvailable reflects the number of new samples available.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent containing two samples
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(5U)
        .WithSkeletonEventData(
            {{kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}});

    // and that a single sample was already retrieved via GetNewSamples
    const std::size_t max_samples{1U};
    score::cpp::ignore = this->GetNewSamples([](auto, auto) noexcept {}, max_samples);

    // When calling GetNumNewSamplesAvailable
    const auto num_new_samples_available_result = this->test_proxy_event_->GetNumNewSamplesAvailable();

    // Then the old data will be invalidated and the returned value will be 0
    ASSERT_TRUE(num_new_samples_available_result.has_value());
    ASSERT_EQ(num_new_samples_available_result.value(), 0U);
}

TEST_F(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsNumberOfAvailableSamplesIgnoringLimitInSubscription)
{
    this->RecordProperty("Verifies", "SCR-21294278");
    this->RecordProperty("Description",
                         "Checks that GetNumNewSamplesAvailable reflects the number of new samples available.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a ProxyEvent that has subscribed to a SkeletonEvent containing two samples with a max sample count smaller
    // than number of samples available
    const std::size_t max_sample_count_subscription{1U};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(max_sample_count_subscription)
        .WithSkeletonEventData(
            {{kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}});

    // When calling GetNumNewSamplesAvailable
    const auto num_new_samples_available_result = this->test_proxy_event_->GetNumNewSamplesAvailable();

    // Then the returned value will be equal to the number of SkeletonEvent samples (ignoring the limit by the max
    // sample count set in subscription)
    ASSERT_TRUE(num_new_samples_available_result.has_value());
    ASSERT_EQ(num_new_samples_available_result.value(), 2U);
}

TEST_F(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsErrorWhenNotSubscribed)
{
    // Given a ProxyEvent that has not subscribed to a SkeletonEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling GetNumNewSamplesAvailable
    const auto num_new_samples = this->test_proxy_event_->GetNumNewSamplesAvailable();

    // Then an error is returned
    ASSERT_FALSE(num_new_samples.has_value());
}

TEST_F(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsNumberOfAvailableSamplesWhenInSubscriptionPending)
{
    // Given a ProxyEvent that is in SubscriptionPending state
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsInSubscriptionPendingWithMaxSamples(5U)
        .WithSkeletonEventData(
            {{kDummySampleValue, kDummyInputTimestamp}, {kDummySampleValue + 1U, kDummyInputTimestamp + 1U}});

    // When calling GetNumNewSamplesAvailable
    const auto num_new_samples_available_result = this->test_proxy_event_->GetNumNewSamplesAvailable();

    // Then the returned value will be equal to the number of SkeletonEvent samples
    ASSERT_TRUE(num_new_samples_available_result.has_value());
    ASSERT_EQ(num_new_samples_available_result.value(), 2U);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
