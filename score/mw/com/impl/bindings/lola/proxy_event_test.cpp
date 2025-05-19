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

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
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

template <typename SampleType>
class CallbackCountingReceiver
{
  public:
    CallbackCountingReceiver(std::uint16_t& num_callbacks_called) : num_callbacks_called_{num_callbacks_called} {}

    void operator()(impl::SamplePtr<SampleType>, const tracing::ITracingRuntime::TracePointDataId)
    {
        num_callbacks_called_++;
    }

  private:
    std::reference_wrapper<std::uint16_t> num_callbacks_called_;
};

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
};
struct GenericProxyEventStruct
{
    using SampleType = void;
    using ProxyEventType = GenericProxyEvent;
};

/// \brief Templated test fixture for ProxyEvent functionality that works for both ProxyEvent and GenericProxyEvent
///
/// \tparam T A tuple containing:
///     SampleType either a type such as std::uint32_t or void
///     ProxyEventType either ProxyEvent or GenericProxyEvent
template <typename T>
class LolaProxyEventFixture : public LolaProxyEventResources
{
  public:
    using SampleType = typename T::SampleType;
    using ProxyEventType = typename T::ProxyEventType;

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

    LolaProxyEventFixture& ThatIsInSubscriptionPendingWithMaxSamples(const std::size_t max_sample_count)
    {
        ThatIsSubscribedWithMaxSamples(max_sample_count);
        const bool is_available = false;
        this->test_proxy_event_->NotifyServiceInstanceChangedAvailability(is_available,
                                                                          ProxyMockedMemoryFixture::kDummyPid);
        const auto new_subscription_state = this->test_proxy_event_->GetSubscriptionState();
        EXPECT_EQ(new_subscription_state, SubscriptionState::kSubscriptionPending);
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
        SCORE_LANGUAGE_FUTURECPP_ASSERT(test_proxy_event_ != nullptr);
        const auto num_samples_available = test_proxy_event_->GetNumNewSamplesAvailable();
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
using LolaProxyEventGetNewSamplesFixture = LolaProxyEventFixture<T>;
TYPED_TEST_SUITE(LolaProxyEventGetNewSamplesFixture, MyTypes, );

template <typename T>
using LolaProxyEventGetNumNewSamplesAvailableFixture = LolaProxyEventFixture<T>;
TYPED_TEST_SUITE(LolaProxyEventGetNumNewSamplesAvailableFixture, MyTypes, );

template <typename T>
using LolaProxyEventDeathFixture = LolaProxyEventFixture<T>;
TYPED_TEST_SUITE(LolaProxyEventDeathFixture, MyTypes, );

TYPED_TEST(LolaProxyEventGetNewSamplesFixture, CallsReceiverForEachAccessibleSample)
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
    CallbackCountingReceiver<typename LolaProxyEventFixture<TypeParam>::SampleType> callback_counting_receiver{
        num_callbacks_called};
    const auto num_callbacks_result = this->GetNewSamples(callback_counting_receiver, max_samples);

    // Then the returned value will be equal to the number of times the callback was called which is once per
    // SkeletonEvent sample
    ASSERT_TRUE(num_callbacks_result.has_value());
    ASSERT_EQ(num_callbacks_result.value(), 2U);
    ASSERT_EQ(num_callbacks_called, 2U);
}

TYPED_TEST(LolaProxyEventGetNewSamplesFixture, CallsReceiverForEachAccessibleSampleLimitedBySubscription)
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
    CallbackCountingReceiver<typename LolaProxyEventFixture<TypeParam>::SampleType> callback_counting_receiver{
        num_callbacks_called};
    const auto num_callbacks_result = this->GetNewSamples(callback_counting_receiver, max_samples);

    // Then the returned value will be equal to the number of times the callback was called which is once per
    // SkeletonEvent sample (limited by the max sample count set in subscription)
    ASSERT_TRUE(num_callbacks_result.has_value());
    ASSERT_EQ(num_callbacks_result.value(), 1U);
    ASSERT_EQ(num_callbacks_called, 1U);
}

TYPED_TEST(LolaProxyEventGetNewSamplesFixture, CallsReceiverForEachAccessibleSampleLimitedByMaxSampleCount)
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
    CallbackCountingReceiver<typename LolaProxyEventFixture<TypeParam>::SampleType> callback_counting_receiver{
        num_callbacks_called};
    const auto num_callbacks_result = this->GetNewSamples(callback_counting_receiver, max_samples);

    // Then the returned value will be equal to the number of times the callback was called which is once per
    // SkeletonEvent sample (limited by the max sample count set in GetNewSamples)
    ASSERT_TRUE(num_callbacks_result.has_value());
    ASSERT_EQ(num_callbacks_result.value(), 1U);
    ASSERT_EQ(num_callbacks_called, 1U);
}

TYPED_TEST(LolaProxyEventGetNewSamplesFixture, CallsReceiverWithDataFromProviderInCorrectOrder)
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
        [&received_samples](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType> sample,
                            const tracing::ITracingRuntime::TracePointDataId timestamp) {
            ASSERT_TRUE(sample);

            const auto value = GetSamplePtrValue(sample.get());
            received_samples.emplace_back(value, timestamp);
        },
        max_samples);

    // Then the data provided to the receiver will be the same data provided by the SkeletonEvent and in the same order
    EXPECT_EQ(values_to_send, received_samples);
}

TYPED_TEST(LolaProxyEventGetNewSamplesFixture, DoNotReceiveEventsFromThePast)
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

TYPED_TEST(LolaProxyEventGetNewSamplesFixture, TransmitEventInShmArea)
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
        [this, slot_index](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType>,
                           const tracing::ITracingRuntime::TracePointDataId timestamp) {
            // Then the retrieved data is pointing to the same valid slot
            const auto& slot = (this->event_control_->data_control)[slot_index];
            EXPECT_FALSE(slot.IsInvalid());
            EXPECT_EQ(slot.GetTimeStamp(), timestamp);
        },
        max_samples);
}

TYPED_TEST(LolaProxyEventGetNewSamplesFixture, ReturnsErrorWhenNotSubscribed)
{
    // Given a ProxyEvent that has not subscribed to a SkeletonEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_)
        .WithSkeletonEventData({{kDummySampleValue, kDummyInputTimestamp}});

    // When calling GetNewSamples
    const std::size_t max_samples{1U};
    const auto num_samples_result = this->GetNewSamples(
        [](impl::SamplePtr<typename LolaProxyEventFixture<TypeParam>::SampleType>, auto) {
            FAIL() << "Callback called despite not having a valid subscription to the event.";
        },
        max_samples);

    // Then an error is returned
    EXPECT_FALSE(num_samples_result.has_value());
}

TYPED_TEST(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsNumberOfAvailableSamples)
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

TYPED_TEST(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsNumberOfAvailableSamplesSinceLastGetNewSamples)
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

TYPED_TEST(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsNumberOfAvailableSamplesIgnoringLimitInSubscription)
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

TYPED_TEST(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsErrorWhenNotSubscribed)
{
    // Given a ProxyEvent that has not subscribed to a SkeletonEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling GetNumNewSamplesAvailable
    const auto num_new_samples = this->test_proxy_event_->GetNumNewSamplesAvailable();

    // Then an error is returned
    ASSERT_FALSE(num_new_samples.has_value());
}

TYPED_TEST(LolaProxyEventGetNumNewSamplesAvailableFixture, ReturnsNumberOfAvailableSamplesWhenInSubscriptionPending)
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

}  // namespace
}  // namespace score::mw::com::impl::lola
