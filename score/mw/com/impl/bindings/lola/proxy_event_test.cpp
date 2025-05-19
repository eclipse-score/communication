/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/proxy_event_common.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/subscription_state.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using TestSampleType = std::uint32_t;

constexpr std::size_t kMaxSampleCount{2U};

constexpr EventSlotStatus::EventTimeStamp kDummyInputTimestamp{10U};
constexpr TestSampleType kDummySampleValue{42U};

/// \brief Function that returns the value pointed to by a pointer
template <typename T>
T GetSamplePtrValue(const T* const sample_ptr)
{
    return *sample_ptr;
}

/// \brief Function that casts and returns the value pointed to by a void pointer
///
/// Assumes that the object in memory being pointed to is of type TestSampleType.
TestSampleType GetSamplePtrValue(const void* const void_ptr)
{
    auto* const typed_ptr = static_cast<const TestSampleType*>(void_ptr);
    return *typed_ptr;
}

/// \brief Structs containing types for templated gtests.
///
/// We use a struct instead of a tuple since a tuple cannot contain a void type.
struct ProxyEventStruct
{
    using SampleType = TestSampleType;
    using ProxyEventType = ProxyEvent<TestSampleType>;
    using ProxyEventAttorneyType = ProxyEventAttorney<TestSampleType>;
};
struct GenericProxyEventStruct
{
    using SampleType = void;
    using ProxyEventType = GenericProxyEvent;
    using ProxyEventAttorneyType = GenericProxyEventAttorney;
};

/// \brief Templated test fixture for ProxyEvent functionality that works for both ProxyEvent and GenericProxyEvent
///
/// \tparam T A tuple containing:
///     SampleType either a type such as std::uint32_t or void
///     ProxyEventType either ProxyEvent or GenericProxyEvent
///     ProxyEventAttorneyType either ProxyEventAttorney or GenericProxyEventAttorney
template <typename T>
class LolaProxyEventFixture : public LolaProxyEventResources
{
  public:
    using SampleType = typename T::SampleType;
    using ProxyEventType = typename T::ProxyEventType;
    using ProxyEventAttorneyType = typename T::ProxyEventAttorneyType;

    LolaProxyEventFixture& GivenAProxyEvent(const ElementFqId element_fq_id, const std::string& event_name)
    {
        test_proxy_event_ = std::make_unique<ProxyEventType>(*proxy_, element_fq_id, event_name);
        return *this;
    }

    LolaProxyEventFixture& ThatIsSubscribedWithMaxSamples(const std::size_t max_sample_count)
    {
        this->test_proxy_event_->Subscribe(max_sample_count);
        return *this;
    }

    LolaProxyEventFixture& WithSkeletonEventData(
        const std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> data)
    {
        for (const auto& datum : data)
        {
            score::cpp::ignore = this->PutData(datum.first, datum.second);
        }
        return *this;
    }

    Result<std::size_t> GetNewSamples(std::function<void(impl::SamplePtr<typename LolaProxyEventFixture<T>::SampleType>,
                                                         const tracing::ITracingRuntime::TracePointDataId)> receiver,
                                      const std::size_t max_num_samples)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(test_proxy_event_ != nullptr);
        TrackerGuardFactory guard_factory{this->sample_reference_tracker_.Allocate(max_num_samples)};
        return test_proxy_event_->GetNewSamples(std::move(receiver), guard_factory);
    }

    bool IsNumNewSamplesAvailableEqualTo(const std::size_t expected_num_samples)
    {
        ProxyEventAttorneyType proxy_event_attorney{*test_proxy_event_};
        const auto num_samples_available = proxy_event_attorney.GetNumNewSamplesAvailableImpl();
        EXPECT_TRUE(num_samples_available.has_value());
        EXPECT_EQ(num_samples_available.value(), expected_num_samples);
        return (num_samples_available.has_value() && (num_samples_available.value() == expected_num_samples));
    }

    std::unique_ptr<ProxyEventType> test_proxy_event_{nullptr};
    SampleReferenceTracker sample_reference_tracker_{kMaxNumSamplesAllowed};
};

// Gtest will run all tests in the LolaProxyEventFixture once for every type, t,
// in MyTypes, such that TypeParam == t for each run.
using MyTypes = ::testing::Types<ProxyEventStruct, GenericProxyEventStruct>;
TYPED_TEST_SUITE(LolaProxyEventFixture, MyTypes, );

template <typename T>
using LolaProxyEventDeathFixture = LolaProxyEventFixture<T>;
TYPED_TEST_SUITE(LolaProxyEventDeathFixture, MyTypes, );

TYPED_TEST(LolaProxyEventFixture, TestGetNewSamples)
{
    this->RecordProperty("Verifies", "SCR-21294278, SCR-14035773, SCR-21350367, SCR-18200533");
    this->RecordProperty("Description",
                         "Checks that GetNewSamples will get new samples from provider and GetNumNewSamplesAvailable "
                         "reflects the number of new samples available. The value of the TracePointDataId will be the "
                         "timestamp of the event slot.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_).ThatIsSubscribedWithMaxSamples(1U);

    const auto slot = this->PutData(kDummySampleValue, kDummyInputTimestamp);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(1));

    const std::size_t max_samples{1U};
    std::uint8_t num_callbacks_called{0U};
    const auto num_callbacks = this->GetNewSamples(
        [this, slot, &num_callbacks_called](
            impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
            const tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);
            EXPECT_FALSE((this->event_control_->data_control)[slot].IsInvalid());

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_EQ(value, kDummySampleValue);
            num_callbacks_called++;

            EXPECT_EQ(timestamp, kDummyInputTimestamp);
        },
        max_samples);
    ASSERT_TRUE(num_callbacks.has_value());
    ASSERT_EQ(num_callbacks.value(), 1);
    ASSERT_EQ(num_callbacks.value(), num_callbacks_called);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(0));
}

TYPED_TEST(LolaProxyEventFixture, ReceiveEventsInOrder)
{
    this->RecordProperty("Verifies", "SCR-21294278, SCR-14035773, SCR-21350367");
    this->RecordProperty("Description",
                         "Sends multiple events and checks that reported number of new samples is correct and they are "
                         "received in order.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> values_to_send{
        {TestSampleType{1U}, EventSlotStatus::EventTimeStamp{1U}},
        {TestSampleType{2U}, EventSlotStatus::EventTimeStamp{2U}},
        {TestSampleType{3U}, EventSlotStatus::EventTimeStamp{3U}}};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(3U)
        .WithSkeletonEventData(values_to_send);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(3));

    const std::size_t max_samples{3U};
    std::uint8_t num_callbacks_called{0U};
    std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> results{};
    EventSlotStatus::EventTimeStamp received_send_time = 1;
    const auto num_callbacks = this->GetNewSamples(
        [&results, &num_callbacks_called, &received_send_time](
            impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
            tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_GE(value, 0);
            EXPECT_LE(value, 3);
            results.push_back({value, timestamp});
            num_callbacks_called++;

            EXPECT_EQ(timestamp, received_send_time);
            received_send_time++;
        },
        max_samples);
    ASSERT_TRUE(num_callbacks.has_value());
    EXPECT_EQ(num_callbacks.value(), 3);
    EXPECT_EQ(num_callbacks.value(), num_callbacks_called);
    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(values_to_send, results);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(0));

    const std::size_t max_samples_2{15U};
    const auto no_new_sample = this->GetNewSamples([](auto, auto) noexcept {}, max_samples_2);
    ASSERT_TRUE(no_new_sample.has_value());
    EXPECT_EQ(no_new_sample.value(), 0);
}

TYPED_TEST(LolaProxyEventFixture, DoNotReceiveEventsFromThePast)
{
    this->RecordProperty("Verifies", "SCR-21294278, SCR-14035773, SCR-21350367");
    this->RecordProperty("Description",
                         "Sends multiple events and checks that reported number of new samples is correct and no "
                         "samples of the past are reported/received.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const EventSlotStatus::EventTimeStamp input_timestamp{17U};
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(2U)
        .WithSkeletonEventData({{kDummySampleValue, input_timestamp}});

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(1));

    const std::size_t max_samples{37U};
    std::uint8_t num_callbacks_called{0U};
    TrackerGuardFactory guard_factory{this->sample_reference_tracker_.Allocate(37U)};
    const auto num_samples = this->GetNewSamples(
        [&num_callbacks_called, input_timestamp](
            impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
            tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_EQ(value, kDummySampleValue);
            num_callbacks_called++;

            EXPECT_EQ(timestamp, input_timestamp);
        },
        max_samples);
    ASSERT_TRUE(num_samples.has_value());
    EXPECT_EQ(num_samples.value(), 1);
    EXPECT_EQ(num_samples.value(), num_callbacks_called);

    constexpr EventSlotStatus::EventTimeStamp input_timestamp_2{1U};
    constexpr TestSampleType input_value_2{kDummySampleValue + 1U};
    this->PutData(input_value_2, input_timestamp_2);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(0));
    const auto new_num_samples = this->GetNewSamples(
        [](auto, auto) {
            FAIL() << "Callback was called although no sample was expected.";
        },
        max_samples);
    ASSERT_TRUE(num_samples.has_value());
    EXPECT_EQ(new_num_samples.value(), 0);
}

TYPED_TEST(LolaProxyEventFixture, GetNewSamplesFailsWhenNotSubscribed)
{
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .WithSkeletonEventData({{kDummySampleValue, kDummyInputTimestamp}});

    const std::size_t max_samples{1U};
    const auto num_samples = this->GetNewSamples(
        [](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType>, auto) noexcept {}, max_samples);
    ASSERT_FALSE(num_samples.has_value());
}

TYPED_TEST(LolaProxyEventFixture, GetNumNewSamplesFailsWhenNotSubscribed)
{
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    const auto num_new_samples = this->test_proxy_event_->GetNumNewSamplesAvailable();
    ASSERT_FALSE(num_new_samples.has_value());
    EXPECT_EQ(num_new_samples.error(), ComErrc::kNotSubscribed);
}

using LoLaTypedProxyEventTestFixture = LolaProxyEventFixture<ProxyEventStruct>;

TEST_F(LoLaTypedProxyEventTestFixture, SampleConstness)
{
    RecordProperty("Verifies", "SCR-6340729");
    RecordProperty("Description", "Proxy shall interpret slot data as const");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    ProxyEventAttorney<TestSampleType> proxy_event_attorney{*test_proxy_event_};
    using SamplesMemberType = typename std::remove_reference<decltype(proxy_event_attorney.GetSamplesMember())>::type;
    static_assert(std::is_const<SamplesMemberType>::value, "Proxy should hold const slot data.");
}

TYPED_TEST(LolaProxyEventFixture, TestProperEventAcquisition)
{
    this->RecordProperty("Verifies", "SCR-5898932, SSR-6225206");
    this->RecordProperty("Description",
                         "Checks whether a proxy is acquiring data from shared memory (req. SCR-5898932) and slot "
                         "referencing works (req. SSR-6225206).");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .ThatIsSubscribedWithMaxSamples(2U)
        .WithSkeletonEventData({{kDummySampleValue, kDummyInputTimestamp}});

    const std::size_t max_samples{1U};
    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kSubscribed);
    auto num_new_samples_avail = this->test_proxy_event_->GetNumNewSamplesAvailable();
    EXPECT_TRUE(num_new_samples_avail.has_value());
    EXPECT_EQ(num_new_samples_avail.value(), 1);
    const auto count = this->GetNewSamples(
        [this](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
               const tracing::ITracingRuntime::TracePointDataId timestamp) {
            EXPECT_EQ(this->sample_reference_tracker_.GetNumAvailableSamples(), kMaxNumSamplesAllowed - 1U);

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_EQ(value, kDummySampleValue);

            EXPECT_EQ(timestamp, kDummyInputTimestamp);
        },
        max_samples);
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count, 1);
    EXPECT_EQ(this->sample_reference_tracker_.GetNumAvailableSamples(), kMaxNumSamplesAllowed);
    this->test_proxy_event_->Unsubscribe();
}

TYPED_TEST(LolaProxyEventFixture, FailOnUnsubscribed)
{
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .WithSkeletonEventData({{kDummySampleValue, kDummyInputTimestamp}});

    auto num_new_samples_avail = this->test_proxy_event_->GetNumNewSamplesAvailable();
    EXPECT_FALSE(num_new_samples_avail.has_value());
    EXPECT_EQ(static_cast<ComErrc>(*(num_new_samples_avail.error())), ComErrc::kNotSubscribed);

    SampleReferenceTracker sample_reference_tracker{2U};
    {
        const std::size_t max_samples{1U};
        const auto count = this->GetNewSamples(
            [](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType>, auto) {
                FAIL() << "Callback called despite not having a valid subscription to the event.";
            },
            max_samples);
        ASSERT_FALSE(count.has_value());
    }
    EXPECT_EQ(sample_reference_tracker.GetNumAvailableSamples(), 2U);
}

TYPED_TEST(LolaProxyEventFixture, TransmitEventInShmArea)
{
    this->RecordProperty("Verifies", "SCR-6367235");
    this->RecordProperty("Description",
                         "A valid SampleAllocateePtr and SamplePtr shall reference a valid and correct slot.");
    this->RecordProperty("TestType ", "Requirements-based test");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_).ThatIsSubscribedWithMaxSamples(1U);

    const EventSlotStatus::EventTimeStamp input_timestamp{1U};
    const auto slot = this->PutData(kDummySampleValue, input_timestamp);

    const std::size_t max_samples{1U};
    SampleReferenceTracker sample_reference_tracker{2U};
    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kSubscribed);
    auto num_new_samples_avail = this->test_proxy_event_->GetNumNewSamplesAvailable();
    EXPECT_TRUE(num_new_samples_avail.has_value());
    EXPECT_EQ(num_new_samples_avail.value(), 1);

    const auto num_samples = this->GetNewSamples(
        [this, slot, input_timestamp](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
                                      const tracing::ITracingRuntime::TracePointDataId timestamp) {
            EXPECT_FALSE((this->event_control_->data_control)[slot].IsInvalid());

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_EQ(value, kDummySampleValue);

            EXPECT_EQ(timestamp, input_timestamp);
        },
        max_samples);
    ASSERT_TRUE(num_samples.has_value());
    EXPECT_EQ(num_samples.value(), 1);

    const auto no_samples = this->GetNewSamples(
        [](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType>, auto) noexcept {}, max_samples);
    ASSERT_TRUE(no_samples.has_value());
    EXPECT_EQ(no_samples.value(), 0);
}

TYPED_TEST(LolaProxyEventFixture, GetBindingType)
{
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    EXPECT_EQ(this->test_proxy_event_->GetBindingType(), BindingType::kLoLa);
}

TYPED_TEST(LolaProxyEventFixture, GetEventSourcePidReturnsPidFromSkeleton)
{
    // Given a mocked Proxy, Skeleton and proxy event
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling GetEventSourcePid
    const auto actual_event_source_pid = this->test_proxy_event_->GetEventSourcePid();

    // Then the pid should be that stored by the skeleton in shared memory
    EXPECT_EQ(actual_event_source_pid, ProxyMockedMemoryFixture::kDummyPid);
}

TYPED_TEST(LolaProxyEventFixture, GetElementFqIdReturnsElementFqIdUsedToCreateProxyEvent)
{
    // Given a mocked Proxy, Skeleton and proxy event
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling GetElementFQId
    const auto actual_element_fq_id = this->test_proxy_event_->GetElementFQId();

    // Then the pid should be that stored by the skeleton in shared memory
    EXPECT_EQ(actual_element_fq_id, this->element_fq_id_);
}

TYPED_TEST(LolaProxyEventFixture, GetMaxSampleCountReturnsEmptyOptionalWhenNotSubscribed)
{
    // Given a mocked Proxy, Skeleton and proxy event which is not currently subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling GetMaxSampleCount
    const auto actual_max_sample_count_result = this->test_proxy_event_->GetMaxSampleCount();

    // Then an empty optional should be returned
    EXPECT_FALSE(actual_max_sample_count_result.has_value());
}

TYPED_TEST(LolaProxyEventFixture, GetMaxSampleCountReturnsMaxSampleCountFromSubscribeCall)
{
    // Given a mocked Proxy, Skeleton and proxy event which is currently subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_).ThatIsSubscribedWithMaxSamples(kMaxSampleCount);

    // When calling GetMaxSampleCount
    const auto actual_max_sample_count_result = this->test_proxy_event_->GetMaxSampleCount();

    // Then the max sample count passed to Subscribe should be returned
    EXPECT_TRUE(actual_max_sample_count_result.has_value());
    EXPECT_EQ(actual_max_sample_count_result.value(), kMaxSampleCount);
}

TYPED_TEST(LolaProxyEventFixture, ProxyEventIsInitallyInNotSubscribedState)
{
    // Given a mocked Proxy, Skeleton and proxy event which is not currently subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling GetSubscriptionState
    const auto new_subscription_state = this->test_proxy_event_->GetSubscriptionState();

    // Then the subscription state will still be not subscribed
    EXPECT_EQ(new_subscription_state, SubscriptionState::kNotSubscribed);
}

TYPED_TEST(LolaProxyEventFixture, CallingSubscribeWillEnterSubscribedState)
{
    // Given a mocked Proxy, Skeleton and proxy event which is subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    this->test_proxy_event_->Subscribe(kMaxSampleCount);

    // When calling GetSubscriptionState
    const auto new_subscription_state = this->test_proxy_event_->GetSubscriptionState();

    // Then the subscription state will be subscribed
    EXPECT_EQ(new_subscription_state, SubscriptionState::kSubscribed);
}

TYPED_TEST(LolaProxyEventFixture,
           CallingNotifyServiceInstanceChangedAvailabilityWithTrueWhenNotSubscribedStaysInNotSubscribed)
{
    // Given a mocked Proxy, Skeleton and proxy event which is not currently subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling NotifyServiceInstanceChangedAvailability with is_available == true
    const bool is_available = true;
    this->test_proxy_event_->NotifyServiceInstanceChangedAvailability(is_available,
                                                                      ProxyMockedMemoryFixture::kDummyPid);

    // Then the subscription state will still be not subscribed
    const auto new_subscription_state = this->test_proxy_event_->GetSubscriptionState();
    EXPECT_EQ(new_subscription_state, SubscriptionState::kNotSubscribed);
}

TYPED_TEST(LolaProxyEventFixture,
           CallingNotifyServiceInstanceChangedAvailabilityWhenSubscribedChangesToSubscriptionPending)
{
    // Given a mocked Proxy, Skeleton and proxy event which is currently subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_).ThatIsSubscribedWithMaxSamples(kMaxSampleCount);

    // When calling NotifyServiceInstanceChangedAvailability with is_available == false
    const bool is_available = false;
    this->test_proxy_event_->NotifyServiceInstanceChangedAvailability(is_available,
                                                                      ProxyMockedMemoryFixture::kDummyPid);

    // Then the subscription state will change to subscription pending
    const auto new_subscription_state = this->test_proxy_event_->GetSubscriptionState();
    EXPECT_EQ(new_subscription_state, SubscriptionState::kSubscriptionPending);
}

TYPED_TEST(LolaProxyEventFixture,
           CallingNotifyServiceInstanceChangedAvailabilityWhenSubscriptionPendingTransitionsToSubscribed)
{
    // Given a mocked Proxy, Skeleton and proxy event which is currently in subscription pending
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_).ThatIsSubscribedWithMaxSamples(kMaxSampleCount);
    const bool is_available = false;
    this->test_proxy_event_->NotifyServiceInstanceChangedAvailability(is_available,
                                                                      ProxyMockedMemoryFixture::kDummyPid);

    // When calling NotifyServiceInstanceChangedAvailability with is_available == true
    const bool new_is_available = true;
    this->test_proxy_event_->NotifyServiceInstanceChangedAvailability(new_is_available,
                                                                      ProxyMockedMemoryFixture::kDummyPid);

    // Then the subscription state will change to subscribed
    const auto new_subscription_state = this->test_proxy_event_->GetSubscriptionState();
    EXPECT_EQ(new_subscription_state, SubscriptionState::kSubscribed);
}

TYPED_TEST(LolaProxyEventDeathFixture, FailOnEventNotFound)
{
    const ElementFqId bad_element_fq_id{0xcdef, 0x6, 0x10, ElementType::EVENT};
    const std::string bad_event_name{"BadEventName"};

    EXPECT_DEATH(score::cpp::ignore = this->GivenAProxyEvent(bad_element_fq_id, bad_event_name), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
