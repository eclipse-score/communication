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
#include "score/mw/com/impl/bindings/lola/proxy_event_common.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"

#include "score/mw/com/impl/subscription_state.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tuple>
#include <type_traits>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using TestSampleType = std::uint32_t;

const std::size_t kMaxSampleCount{2U};

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
  protected:
    using SampleType = typename T::SampleType;
    using ProxyEventType = typename T::ProxyEventType;
    using ProxyEventAttorneyType = typename T::ProxyEventAttorneyType;

    bool IsNumNewSamplesAvailableEqualTo(const std::size_t expected_num_samples)
    {
        const auto num_samples_available = proxy_event_attorney_.GetNumNewSamplesAvailableImpl();
        EXPECT_TRUE(num_samples_available.has_value());
        EXPECT_EQ(num_samples_available.value(), expected_num_samples);
        return (num_samples_available.has_value() && (num_samples_available.value() == expected_num_samples));
    }

    ProxyEventType test_proxy_event_{*proxy_, element_fq_id_, event_name_};
    ProxyEventAttorneyType proxy_event_attorney_{test_proxy_event_};
    ProxyEventCommon& proxy_event_common_{proxy_event_attorney_.GetProxyEventCommon()};
    ProxyEventCommonAttorney proxy_event_common_attorney_{proxy_event_common_};
    SampleReferenceTracker sample_reference_tracker_{100U};
};

// Gtest will run all tests in the LolaProxyEventFixture once for every type, t,
// in MyTypes, such that TypeParam == t for each run.
using MyTypes = ::testing::Types<ProxyEventStruct, GenericProxyEventStruct>;
TYPED_TEST_SUITE(LolaProxyEventFixture, MyTypes, );

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

    const EventSlotStatus::EventTimeStamp input_timestamp{10U};
    const std::uint32_t input_value{42U};
    const auto slot = this->PutData(input_value, input_timestamp);

    const std::size_t max_slots{1U};
    this->test_proxy_event_.Subscribe(max_slots);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(1));

    std::uint8_t num_callbacks_called{0U};
    TrackerGuardFactory guard_factory{this->sample_reference_tracker_.Allocate(1U)};
    const auto num_callbacks = this->proxy_event_attorney_.GetNewSamplesImpl(
        [this, slot, &num_callbacks_called, input_value, input_timestamp](
            impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
            const tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);
            EXPECT_FALSE((this->event_control_->data_control)[slot].IsInvalid());

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_EQ(value, input_value);
            num_callbacks_called++;

            EXPECT_EQ(timestamp, input_timestamp);
        },
        guard_factory);
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

    EventSlotStatus::EventTimeStamp send_time{1};
    const std::vector<TestSampleType> values_to_send{1, 2, 3};
    for (const auto value_to_send : values_to_send)
    {
        this->PutData(value_to_send, send_time);
        send_time++;
    }

    const std::size_t max_slots{3U};
    this->test_proxy_event_.Subscribe(max_slots);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(3));

    std::uint8_t num_callbacks_called{0U};
    std::vector<TestSampleType> results{};
    TrackerGuardFactory guard_factory{this->sample_reference_tracker_.Allocate(3U)};
    EventSlotStatus::EventTimeStamp received_send_time = 1;
    const auto num_callbacks = this->proxy_event_attorney_.GetNewSamplesImpl(
        [&results, &num_callbacks_called, &received_send_time](
            impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
            tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_GE(value, 0);
            EXPECT_LE(value, 3);
            results.push_back(value);
            num_callbacks_called++;

            EXPECT_EQ(timestamp, received_send_time);
            received_send_time++;
        },
        guard_factory);
    ASSERT_TRUE(num_callbacks.has_value());
    EXPECT_EQ(num_callbacks.value(), 3);
    EXPECT_EQ(num_callbacks.value(), num_callbacks_called);
    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(values_to_send, results);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(0));

    TrackerGuardFactory guard_factory_{this->sample_reference_tracker_.Allocate(15U)};
    const auto no_new_sample =
        this->proxy_event_attorney_.GetNewSamplesImpl([](auto, auto) noexcept {}, guard_factory_);
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

    const std::size_t max_slots{2U};
    this->test_proxy_event_.Subscribe(max_slots);

    const EventSlotStatus::EventTimeStamp input_timestamp{17U};
    const std::uint32_t input_value{42U};
    this->PutData(input_value, input_timestamp);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(1));

    std::uint8_t num_callbacks_called{0U};
    TrackerGuardFactory guard_factory{this->sample_reference_tracker_.Allocate(37U)};
    const auto num_samples = this->proxy_event_attorney_.GetNewSamplesImpl(
        [&num_callbacks_called, input_value, input_timestamp](
            impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
            tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_EQ(value, input_value);
            num_callbacks_called++;

            EXPECT_EQ(timestamp, input_timestamp);
        },
        guard_factory);
    ASSERT_TRUE(num_samples.has_value());
    EXPECT_EQ(num_samples.value(), 1);
    EXPECT_EQ(num_samples.value(), num_callbacks_called);

    const EventSlotStatus::EventTimeStamp input_timestscore_future_cpp_2{1U};
    const std::uint32_t input_value_2{42U};
    this->PutData(input_value_2, input_timestscore_future_cpp_2);

    EXPECT_TRUE(this->IsNumNewSamplesAvailableEqualTo(0));
    TrackerGuardFactory new_guard_factory{this->sample_reference_tracker_.Allocate(37U)};
    const auto new_num_samples = this->proxy_event_attorney_.GetNewSamplesImpl(
        [](auto, auto) {
            FAIL() << "Callback was called although no sample was expected.";
        },
        new_guard_factory);
    ASSERT_TRUE(num_samples.has_value());
    EXPECT_EQ(new_num_samples.value(), 0);
}

TYPED_TEST(LolaProxyEventFixture, GetNewSamplesFailsWhenNotSubscribed)
{
    const std::uint32_t input_value{42U};
    this->PutData(input_value);

    TrackerGuardFactory guard_factory{this->sample_reference_tracker_.Allocate(1U)};
    const auto num_samples = this->test_proxy_event_.GetNewSamples(
        [](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType>, auto) noexcept {}, guard_factory);
    ASSERT_FALSE(num_samples.has_value());
}

TYPED_TEST(LolaProxyEventFixture, GetNumNewSamplesFailsWhenNotSubscribed)
{
    const auto num_new_samples = this->test_proxy_event_.GetNumNewSamplesAvailable();
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

    using SamplesMemberType = typename std::remove_reference<decltype(proxy_event_attorney_.GetSamplesMember())>::type;
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

    const std::size_t max_sample_count{2U};
    const EventSlotStatus::EventTimeStamp input_timestamp{10U};
    const std::uint32_t input_value{42U};
    this->PutData(input_value, input_timestamp);

    SampleReferenceTracker sample_reference_tracker{2U};
    TrackerGuardFactory guard_factory{sample_reference_tracker.Allocate(1U)};
    this->test_proxy_event_.Subscribe(max_sample_count);
    EXPECT_EQ(this->test_proxy_event_.GetSubscriptionState(), SubscriptionState::kSubscribed);
    auto num_new_samples_avail = this->test_proxy_event_.GetNumNewSamplesAvailable();
    EXPECT_TRUE(num_new_samples_avail.has_value());
    EXPECT_EQ(num_new_samples_avail.value(), 1);
    const Result<std::size_t> count = this->test_proxy_event_.GetNewSamples(
        [&sample_reference_tracker, input_value, input_timestamp](
            impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
            const tracing::ITracingRuntime::TracePointDataId timestamp) {
            EXPECT_EQ(sample_reference_tracker.GetNumAvailableSamples(), 1U);

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_EQ(value, input_value);

            EXPECT_EQ(timestamp, input_timestamp);
        },
        guard_factory);
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count, 1);
    EXPECT_EQ(sample_reference_tracker.GetNumAvailableSamples(), 2U);
    this->test_proxy_event_.Unsubscribe();
}

TYPED_TEST(LolaProxyEventFixture, FailOnUnsubscribed)
{
    this->PutData();

    auto num_new_samples_avail = this->test_proxy_event_.GetNumNewSamplesAvailable();
    EXPECT_FALSE(num_new_samples_avail.has_value());
    EXPECT_EQ(static_cast<ComErrc>(*(num_new_samples_avail.error())), ComErrc::kNotSubscribed);

    SampleReferenceTracker sample_reference_tracker{2U};
    {
        TrackerGuardFactory guard_factory{sample_reference_tracker.Allocate(1U)};
        const Result<std::size_t> count = this->test_proxy_event_.GetNewSamples(
            [](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType>, auto) {
                FAIL() << "Callback called despite not having a valid subscription to the event.";
            },
            guard_factory);
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

    const EventSlotStatus::EventTimeStamp input_timestamp{1U};
    const std::uint32_t input_value{42U};
    const auto slot = this->PutData(input_value, input_timestamp);

    const std::size_t max_sample_count{1U};

    SampleReferenceTracker sample_reference_tracker{2U};
    TrackerGuardFactory guard_factory{sample_reference_tracker.Allocate(1U)};
    this->test_proxy_event_.Subscribe(max_sample_count);
    EXPECT_EQ(this->test_proxy_event_.GetSubscriptionState(), SubscriptionState::kSubscribed);
    auto num_new_samples_avail = this->test_proxy_event_.GetNumNewSamplesAvailable();
    EXPECT_TRUE(num_new_samples_avail.has_value());
    EXPECT_EQ(num_new_samples_avail.value(), 1);

    const auto num_samples = this->proxy_event_attorney_.GetNewSamplesImpl(
        [this, slot, input_value, input_timestamp](
            impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
            const tracing::ITracingRuntime::TracePointDataId timestamp) {
            EXPECT_FALSE((this->event_control_->data_control)[slot].IsInvalid());

            const auto value = GetSamplePtrValue(sample.get());
            EXPECT_EQ(value, input_value);

            EXPECT_EQ(timestamp, input_timestamp);
        },
        guard_factory);
    ASSERT_TRUE(num_samples.has_value());
    EXPECT_EQ(num_samples.value(), 1);

    TrackerGuardFactory guard_factory2{sample_reference_tracker.Allocate(1U)};
    const auto no_samples = this->proxy_event_attorney_.GetNewSamplesImpl(
        [](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType>, auto) noexcept {}, guard_factory2);
    ASSERT_TRUE(no_samples.has_value());
    EXPECT_EQ(no_samples.value(), 0);
}

TYPED_TEST(LolaProxyEventFixture, GetBindingType)
{
    EXPECT_EQ(this->test_proxy_event_.GetBindingType(), BindingType::kLoLa);
}

TYPED_TEST(LolaProxyEventFixture, GetEventSourcePidReturnsPidFromSkeleton)
{
    // Given a mocked Proxy, Skeleton and proxy event

    // When calling GetEventSourcePid
    const auto actual_event_source_pid = this->test_proxy_event_.GetEventSourcePid();

    // Then the pid should be that stored by the skeleton in shared memory
    EXPECT_EQ(actual_event_source_pid, ProxyMockedMemoryFixture::kDummyPid);
}

TYPED_TEST(LolaProxyEventFixture, GetElementFqIdReturnsElementFqIdUsedToCreateProxyEvent)
{
    // Given a mocked Proxy, Skeleton and proxy event

    // When calling GetElementFQId
    const auto actual_element_fq_id = this->test_proxy_event_.GetElementFQId();

    // Then the pid should be that stored by the skeleton in shared memory
    EXPECT_EQ(actual_element_fq_id, this->element_fq_id_);
}

TYPED_TEST(LolaProxyEventFixture, GetMaxSampleCountReturnsEmptyOptionalWhenNotSubscribed)
{
    // Given a mocked Proxy, Skeleton and proxy event which is not currently subscribed

    // When calling GetMaxSampleCount
    const auto actual_max_sample_count_result = this->test_proxy_event_.GetMaxSampleCount();

    // Then an empty optional should be returned
    EXPECT_FALSE(actual_max_sample_count_result.has_value());
}

TYPED_TEST(LolaProxyEventFixture, GetMaxSampleCountReturnsMaxSampleCountFromSubscribeCall)
{
    // Given a mocked Proxy, Skeleton and proxy event which is currently subscribed
    this->test_proxy_event_.Subscribe(kMaxSampleCount);

    // When calling GetMaxSampleCount
    const auto actual_max_sample_count_result = this->test_proxy_event_.GetMaxSampleCount();

    // Then the max sample count passed to Subscribe should be returned
    EXPECT_TRUE(actual_max_sample_count_result.has_value());
    EXPECT_EQ(actual_max_sample_count_result.value(), kMaxSampleCount);
}

TYPED_TEST(LolaProxyEventFixture, ProxyEventIsInitallyInNotSubscribedState)
{
    // Given a mocked Proxy, Skeleton and proxy event which is not currently subscribed

    // When calling GetSubscriptionState
    const auto new_subscription_state = this->test_proxy_event_.GetSubscriptionState();

    // Then the subscription state will still be not subscribed
    EXPECT_EQ(new_subscription_state, SubscriptionState::kNotSubscribed);
}

TYPED_TEST(LolaProxyEventFixture, CallingSubscribeWillEnterSubscribedState)
{
    // Given a mocked Proxy, Skeleton and proxy event which is subscribed
    this->test_proxy_event_.Subscribe(kMaxSampleCount);

    // When calling GetSubscriptionState
    const auto new_subscription_state = this->test_proxy_event_.GetSubscriptionState();

    // Then the subscription state will be subscribed
    EXPECT_EQ(new_subscription_state, SubscriptionState::kSubscribed);
}

TYPED_TEST(LolaProxyEventFixture,
           CallingNotifyServiceInstanceChangedAvailabilityWithTrueWhenNotSubscribedStaysInNotSubscribed)
{
    // Given a mocked Proxy, Skeleton and proxy event which is not currently subscribed

    // When calling NotifyServiceInstanceChangedAvailability with is_available == true
    const bool is_available = true;
    this->test_proxy_event_.NotifyServiceInstanceChangedAvailability(is_available, ProxyMockedMemoryFixture::kDummyPid);

    // Then the subscription state will still be not subscribed
    const auto new_subscription_state = this->test_proxy_event_.GetSubscriptionState();
    EXPECT_EQ(new_subscription_state, SubscriptionState::kNotSubscribed);
}

TYPED_TEST(LolaProxyEventFixture,
           CallingNotifyServiceInstanceChangedAvailabilityWhenSubscribedChangesToSubscriptionPending)
{
    // Given a mocked Proxy, Skeleton and proxy event which is currently subscribed
    this->test_proxy_event_.Subscribe(kMaxSampleCount);

    // When calling NotifyServiceInstanceChangedAvailability with is_available == false
    const bool is_available = false;
    this->test_proxy_event_.NotifyServiceInstanceChangedAvailability(is_available, ProxyMockedMemoryFixture::kDummyPid);

    // Then the subscription state will change to subscription pending
    const auto new_subscription_state = this->test_proxy_event_.GetSubscriptionState();
    EXPECT_EQ(new_subscription_state, SubscriptionState::kSubscriptionPending);
}

TYPED_TEST(LolaProxyEventFixture,
           CallingNotifyServiceInstanceChangedAvailabilityWhenSubscriptionPendingTransitionsToSubscribed)
{
    // Given a mocked Proxy, Skeleton and proxy event which is currently in subscription pending
    this->test_proxy_event_.Subscribe(kMaxSampleCount);
    const bool is_available = false;
    this->test_proxy_event_.NotifyServiceInstanceChangedAvailability(is_available, ProxyMockedMemoryFixture::kDummyPid);

    // When calling NotifyServiceInstanceChangedAvailability with is_available == true
    const bool new_is_available = true;
    this->test_proxy_event_.NotifyServiceInstanceChangedAvailability(new_is_available,
                                                                     ProxyMockedMemoryFixture::kDummyPid);

    // Then the subscription state will change to subscribed
    const auto new_subscription_state = this->test_proxy_event_.GetSubscriptionState();
    EXPECT_EQ(new_subscription_state, SubscriptionState::kSubscribed);
}

using LolaProxyEventDeathFixture = LolaProxyEventResources;
TEST_F(LolaProxyEventDeathFixture, FailOnEventNotFound)
{
    const ElementFqId bad_element_fq_id{0xcdef, 0x6, 0x10, ElementType::EVENT};
    const std::string bad_event_name{"BadEventName"};

    EXPECT_DEATH(ProxyEvent<std::uint8_t>(*proxy_, bad_element_fq_id, bad_event_name), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
