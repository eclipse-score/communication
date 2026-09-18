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
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/sample_reference_tracker.h"
#include "score/mw/com/impl/subscription_state.h"

#include "score/language/safecpp/scoped_function/scope.h"

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

class ProxyTestAttorney
{
  public:
    explicit ProxyTestAttorney(Proxy& proxy) : proxy_{proxy} {}

    const std::unordered_map<std::string_view, std::reference_wrapper<ProxyEvent>>& GetEvents() const
    {
        return proxy_.proxy_events_;
    }

  private:
    Proxy& proxy_;
};

namespace
{

using ::testing::_;

using TestSampleType = std::uint32_t;

constexpr std::size_t kMaxSampleCount{2U};

constexpr EventSlotStatus::EventTimeStamp kDummyInputTimestamp{10U};
constexpr TestSampleType kDummySampleValue{42U};

class CallbackCountingReceiver
{
  public:
    CallbackCountingReceiver(std::uint16_t& num_callbacks_called) : num_callbacks_called_{num_callbacks_called} {}

    void operator()(impl::SamplePtr<void>, const tracing::ITracingRuntime::TracePointDataId)
    {
        num_callbacks_called_++;
    }

  private:
    std::reference_wrapper<std::uint16_t> num_callbacks_called_;
};

/// \brief Function that casts and returns the value pointed to by a void pointer
///
/// Assumes that the object in memory being pointed to is of type TestSampleType.
TestSampleType GetSamplePtrValue(const void* const void_ptr)
{
    auto* const typed_ptr = static_cast<const TestSampleType*>(void_ptr);
    return *typed_ptr;
}

template <typename R, typename... Args>
std::shared_ptr<ScopedEventReceiveHandler> FromMockFunction(safecpp::Scope<>& event_receive_handler_scope,
                                                            ::testing::MockFunction<R(Args...)>& mock_function)
{
    return std::make_shared<ScopedEventReceiveHandler>(event_receive_handler_scope, [&mock_function](Args&&... args) {
        mock_function.Call(std::forward<Args>(args)...);
    });
}

/// \brief Test fixture for ProxyEvent functionality.
class LolaProxyEventFixture : public LolaProxyEventResources
{
  public:
    LolaProxyEventFixture& GivenAProxyEvent(const ElementFqId element_fq_id, const std::string& event_name)
    {
        test_proxy_event_ = std::make_unique<ProxyEvent>(*proxy_, element_fq_id, event_name);
        return *this;
    }

    LolaProxyEventFixture& ThatIsSubscribedWithMaxSamples(const std::size_t max_sample_count)
    {
        std::ignore = this->test_proxy_event_->Subscribe(max_sample_count);
        sample_reference_tracker_ = std::make_unique<SampleReferenceTracker>(max_sample_count);
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

    Result<std::size_t> GetNewSamples(
        std::function<void(impl::SamplePtr<void>, const tracing::ITracingRuntime::TracePointDataId)> receiver,
        const std::size_t max_num_samples)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(test_proxy_event_ != nullptr);
        SCORE_LANGUAGE_FUTURECPP_ASSERT(sample_reference_tracker_ != nullptr);
        TrackerGuardFactory guard_factory{this->sample_reference_tracker_->Allocate(max_num_samples)};
        return test_proxy_event_->GetNewSamples(std::move(receiver), guard_factory);
    }

    // ProxyEvent no longer Unsubscribes on destruction, so do it explicitly.
    void TearDown() override
    {
        if (test_proxy_event_ != nullptr)
        {
            test_proxy_event_->Unsubscribe();
        }
        ProxyMockedMemoryFixture::TearDown();
    }

    std::unique_ptr<ProxyEvent> test_proxy_event_{nullptr};
    std::unique_ptr<SampleReferenceTracker> sample_reference_tracker_{};
};

using LolaProxyEventConstructionFixture = LolaProxyEventFixture;
using LolaProxyEventGetNewSamplesFixture = LolaProxyEventFixture;
using LolaProxyEventGetNumNewSamplesAvailableFixture = LolaProxyEventFixture;
using LolaProxyEventDeathTest = LolaProxyEventFixture;

TEST_F(LolaProxyEventConstructionFixture, ConstructingRegistersEventWithParent)
{
    // Given a proxy with an events map which is initially empty
    auto& events_map = ProxyTestAttorney(*this->proxy_).GetEvents();
    ASSERT_EQ(events_map.size(), 0U);

    // When constructing a ProxyEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // Then the event should have been registered in the parent Proxy's map of events
    EXPECT_EQ(events_map.size(), 1U);
    auto registered_event_it = events_map.find(this->event_name_);
    EXPECT_NE(registered_event_it, events_map.end());
    EXPECT_EQ(&registered_event_it->second.get(), &(*this->test_proxy_event_));
}

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

TEST_F(LolaProxyEventFixture, GetBindingType)
{
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    EXPECT_EQ(this->test_proxy_event_->GetBindingType(), BindingType::kLoLa);
}

TEST_F(LolaProxyEventFixture, GetElementFqIdReturnsElementFqIdUsedToCreateProxyEvent)
{
    // Given a mocked Proxy, Skeleton and proxy event
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling GetElementFQId
    const auto actual_element_fq_id = this->test_proxy_event_->GetElementFQId();

    // Then the pid should be that stored by the skeleton in shared memory
    EXPECT_EQ(actual_element_fq_id, this->element_fq_id_);
}

TEST_F(LolaProxyEventFixture, GetMaxSampleCountReturnsEmptyOptionalWhenNotSubscribed)
{
    // Given a mocked Proxy, Skeleton and proxy event which is not currently subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling GetMaxSampleCount
    const auto actual_max_sample_count_result = this->test_proxy_event_->GetMaxSampleCount();

    // Then an empty optional should be returned
    EXPECT_FALSE(actual_max_sample_count_result.has_value());
}

TEST_F(LolaProxyEventFixture, GetMaxSampleCountReturnsMaxSampleCountFromSubscribeCall)
{
    // Given a mocked Proxy, Skeleton and proxy event which is currently subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_).ThatIsSubscribedWithMaxSamples(kMaxSampleCount);

    // When calling GetMaxSampleCount
    const auto actual_max_sample_count_result = this->test_proxy_event_->GetMaxSampleCount();

    // Then the max sample count passed to Subscribe should be returned
    EXPECT_TRUE(actual_max_sample_count_result.has_value());
    EXPECT_EQ(actual_max_sample_count_result.value(), kMaxSampleCount);
}

TEST_F(LolaProxyEventFixture, ProxyEventIsInitallyInNotSubscribedState)
{
    // Given a mocked Proxy, Skeleton and proxy event which is not currently subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When calling GetSubscriptionState
    const auto new_subscription_state = this->test_proxy_event_->GetSubscriptionState();

    // Then the subscription state will still be not subscribed
    EXPECT_EQ(new_subscription_state, SubscriptionState::kNotSubscribed);
}

TEST_F(LolaProxyEventFixture, CallingSubscribeWillEnterSubscribedState)
{
    // Given a mocked Proxy, Skeleton and proxy event which is subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    ASSERT_TRUE(this->test_proxy_event_->Subscribe(kMaxSampleCount));

    // When calling GetSubscriptionState
    const auto new_subscription_state = this->test_proxy_event_->GetSubscriptionState();

    // Then the subscription state will be subscribed
    EXPECT_EQ(new_subscription_state, SubscriptionState::kSubscribed);
}

TEST_F(LolaProxyEventFixture,
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

TEST_F(LolaProxyEventFixture, CallingNotifyServiceInstanceChangedAvailabilityWhenSubscribedChangesToSubscriptionPending)
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

TEST_F(LolaProxyEventFixture,
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

TEST_F(LolaProxyEventDeathTest, FailOnEventNotFound)
{
    const ElementFqId bad_element_fq_id{0xcdef, 0x6, 0x10, ServiceElementType::EVENT};
    const std::string bad_event_name{"BadEventName"};

    EXPECT_DEATH(score::cpp::ignore = this->GivenAProxyEvent(bad_element_fq_id, bad_event_name), ".*");
}

TEST_F(LolaProxyEventFixture, GetDataTypeSizeInfo)
{
    RecordProperty("lobster-tracing", "GenericProxyEventGetDataTypeSizeInfo");
    RecordProperty(
        "Description",
        "Checks that GetDataTypeSizeInfo will return the data type size info of the underlying event data type.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid ProxyEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // Expect, that asking about the Sample size, we get the sizeof the underlying event data type (which is
    // TestSampleType in case of LolaProxyEventResources)
    EXPECT_EQ(this->test_proxy_event_->GetDataTypeSizeInfo().Alignment(), alignof(SampleType));
    EXPECT_EQ(this->test_proxy_event_->GetDataTypeSizeInfo().Size(), sizeof(SampleType));
}

TEST_F(LolaProxyEventFixture, HasSerializedFormat)
{
    RecordProperty("lobster-tracing", "Communication.GenericProxyEventHasSerializedFormat");
    RecordProperty("Description", "Checks that HasSerializedFormat will always return false for the Lola binding.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given a valid ProxyEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // Expect, that asking about the serialized format, we get "FALSE"
    EXPECT_EQ(this->test_proxy_event_->HasSerializedFormat(), false);
}

TEST_F(LolaProxyEventFixture, HoldsEventMetaInfoAsConstReference)
{
    // Given a ProxyEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When accessing the proxy event through the test attorney
    ProxyEventAttorney proxy_event_attorney{*test_proxy_event_};

    // Then the proxy stores EventMetaInfo as const data
    using MetaInfoMemberType = typename std::remove_reference<decltype(proxy_event_attorney.GetMetaInfoMember())>::type;
    static_assert(std::is_const<MetaInfoMemberType>::value, "Proxy should hold const event meta info.");
}

class LolaProxyEventSubscriptionScopeFixture : public LolaProxyEventFixture
{
  public:
    void ExpectCallbackRegistration()
    {
        constexpr const IMessagePassingService::HandlerRegistrationNoType my_handler_no = 37U;

        EXPECT_CALL(event_handler_, Call());

        EXPECT_CALL(*mock_service_, RegisterEventNotification(QualityType::kASIL_QM, element_fq_id_, _, kDummyPid))
            .WillOnce(
                ::testing::Invoke([&](auto, auto, std::weak_ptr<ScopedEventReceiveHandler> handler_weak_ptr, auto) {
                    auto handler_shared_ptr = handler_weak_ptr.lock();
                    EXPECT_TRUE(handler_shared_ptr);
                    if (handler_shared_ptr)
                    {
                        (*handler_shared_ptr)();
                    }
                    return my_handler_no;
                }));
        EXPECT_CALL(*mock_service_,
                    UnregisterEventNotification(QualityType::kASIL_QM, element_fq_id_, my_handler_no, kDummyPid));
    }

    ::testing::MockFunction<void()> event_handler_{};
};

TEST_F(LolaProxyEventSubscriptionScopeFixture, RegisterEventHandlerBeforeSubscription)
{
    safecpp::Scope<> event_receive_handler_scope{};

    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    const std::size_t max_sample_count{1U};
    this->ExpectCallbackRegistration();
    auto mocked_receive_handler = FromMockFunction(event_receive_handler_scope, this->event_handler_);
    ASSERT_TRUE(this->test_proxy_event_->SetReceiveHandler(mocked_receive_handler));
    ASSERT_TRUE(this->test_proxy_event_->Subscribe(max_sample_count));
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, RegisterEventHandlerAfterSubscription)
{
    safecpp::Scope<> event_receive_handler_scope{};

    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    const std::size_t max_sample_count{1U};
    this->ExpectCallbackRegistration();
    ASSERT_TRUE(this->test_proxy_event_->Subscribe(max_sample_count));
    ASSERT_TRUE(this->test_proxy_event_->SetReceiveHandler(
        FromMockFunction(event_receive_handler_scope, this->event_handler_)));
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, DoNotRegisterEventHandler)
{
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    ASSERT_TRUE(this->test_proxy_event_->Subscribe(1U));

    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kSubscribed);
}

TEST_F(LolaProxyEventSubscriptionScopeFixture,
       CallingSetReceiveHandlerRegistersEventNotificationWithPidFromSharedMemory)
{
    const std::size_t max_sample_count{1U};

    // Expecting that a receive handler will be registered with the pid that was written to shared memory by the
    // skeleton
    EXPECT_CALL(*this->mock_service_, RegisterEventNotification(QualityType::kASIL_QM, element_fq_id_, _, kDummyPid));

    // Given a subscribed ProxyEvent
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    std::ignore = this->test_proxy_event_->Subscribe(max_sample_count);

    // When registering a receive handler
    safecpp::Scope<> event_receive_handler_scope{};
    std::ignore =
        this->test_proxy_event_->SetReceiveHandler(FromMockFunction(event_receive_handler_scope, this->event_handler_));
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, SubscriptionFailsWhenProviderRejectsSubscription)
{
    this->RecordProperty("lobster-tracing",
                         "Communication.ProxyFieldSubscribe, Communication.ProxyEventSubscribe, "
                         "Communication.GenericProxyEventSubscribe, Communication.BehaviourOfSubscribe");
    this->RecordProperty("Description",
                         "Checks that a subscription will fail when the provider rejects the subscription due to "
                         "overflowed max sample count.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When we subscribe requesting too many samples
    const auto subscribe_result = this->test_proxy_event_->Subscribe(max_num_slots_ + 1U);

    // Then the subscribe call should return an error
    ASSERT_FALSE(subscribe_result.has_value());
    EXPECT_EQ(subscribe_result.error(), ComErrc::kMaxSampleCountNotRealizable);

    // And we stay in not subscribed state
    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kNotSubscribed);
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, UnsubscribeImmediatelyAfterSubscribing)
{
    this->RecordProperty("lobster-tracing",
                         "Communication.ProxyFieldUnsubscribe, Communication.ProxyEventUnsubscribe, "
                         "Communication.GenericProxyEventUnsubscribe, Communication.BehaviourOfUnsubscribe");
    this->RecordProperty("Description",
                         "Unsubscribe will be successfully processed if a user unsubscribes from an event immediately "
                         "after subscribing.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_sample_count{1U};

    // Given a proxy that unsubscribes while waiting for being subscribed correctly
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When we subscribe (sending a subscribe message to the producer)
    std::ignore = this->test_proxy_event_->Subscribe(max_sample_count);

    // and we unsubscribe before the producer sends a response that it has changed state
    this->test_proxy_event_->Unsubscribe();

    // And we stay in not subscribed state
    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kNotSubscribed);
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, UnsubscribingWillUnregisterEventHandler)
{
    // SCR-20236391 and SCR-20237033 are split with CallsUnsubscribeOnDestruction in traits_test.cpp.
    // this test covers Unsubscribe triggering UnregisterEventNotification, the other covers proxy
    // destruction triggering Unsubscribe on the events and fields.
    this->RecordProperty("Verifies", "SCR-21293524, SCR-20236391");
    this->RecordProperty("lobster-tracing", "Communication.GenericProxyEventDestructor");
    this->RecordProperty(
        "Description",
        "Checks that calling Unsubscribe while currently subscribed will unregister a registered event "
        "receive handler.");
    this->RecordProperty("TestType", "Requirements-based test");
    this->RecordProperty("Priority", "1");
    this->RecordProperty("DerivationTechnique", "Analysis of requirements");

    const std::size_t max_sample_count{1U};
    constexpr const IMessagePassingService::HandlerRegistrationNoType my_handler_no = 37U;
    bool handler_unregistered{false};

    // Expecting that a receive handler will be registered
    EXPECT_CALL(*this->mock_service_, RegisterEventNotification(QualityType::kASIL_QM, element_fq_id_, _, kDummyPid))
        .WillOnce(::testing::Return(my_handler_no));

    // and the same receive handler will be unregistered
    EXPECT_CALL(*this->mock_service_,
                UnregisterEventNotification(QualityType::kASIL_QM, element_fq_id_, my_handler_no, kDummyPid))
        .WillOnce(::testing::InvokeWithoutArgs([&handler_unregistered]() {
            handler_unregistered = true;
        }));

    // Given a proxy that unsubscribes while waiting for being subscribed correctly
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When we subscribe (sending a subscribe message to the producer)
    std::ignore = this->test_proxy_event_->Subscribe(max_sample_count);

    // and Register a receive handler
    safecpp::Scope<> event_receive_handler_scope{};
    std::ignore =
        this->test_proxy_event_->SetReceiveHandler(FromMockFunction(event_receive_handler_scope, this->event_handler_));

    // Then the receive handler should not be unregistered
    EXPECT_FALSE(handler_unregistered);

    // and when the ProxyEvent unsubscribes
    this->test_proxy_event_->Unsubscribe();

    // Then the receive handler should be unregistered
    EXPECT_TRUE(handler_unregistered);
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, DoubleSubscribe)
{
    const std::size_t max_sample_count{max_num_slots_ / 2U};

    // Given a valid proxy that is already subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    const auto subscribe_result = this->test_proxy_event_->Subscribe(max_sample_count);
    EXPECT_TRUE(subscribe_result.has_value());

    // When subscribing again
    const auto subscribe_result_2 = this->test_proxy_event_->Subscribe(max_sample_count);
    EXPECT_TRUE(subscribe_result_2.has_value());

    // We don't crash
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, DoubleSubscribeWithDifferentMaxSampleCount)
{
    // Given a valid proxy that is already subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    const auto subscribe_result = this->test_proxy_event_->Subscribe(max_num_slots_ - 1U);
    EXPECT_TRUE(subscribe_result.has_value());

    // When subscribing again with a different sample count
    const auto subscribe_result_2 = this->test_proxy_event_->Subscribe(1U);
    ASSERT_FALSE(subscribe_result_2.has_value());
    EXPECT_EQ(subscribe_result_2.error(), ComErrc::kMaxSampleCountNotRealizable);

    // We don't crash
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, UnsetReceiveHandlerWhileSubscribed)
{
    // Given a valid proxy where we are only subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    ASSERT_TRUE(this->test_proxy_event_->Subscribe(1U));

    // When removing an receive handler
    const auto action = [this]() {
        score::cpp::ignore = this->test_proxy_event_->UnsetReceiveHandler();
    };

    // Then we don't crash
    EXPECT_NO_FATAL_FAILURE(action());
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, UnsetReceiveHandlerWithoutBeingSubscribed)
{
    // Given a valid proxy that is not subscribed
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);

    // When removing an receive handler
    const auto action = [this]() {
        std::ignore = this->test_proxy_event_->UnsetReceiveHandler();
    };

    // Then we don't crash
    EXPECT_NO_FATAL_FAILURE(action());
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, RegisterSubscriptionStateChangeHandler)
{
    // Given a valid proxy with state change callback (persistent) that is not subscribed yet
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    SubscriptionState last_subscription_state = SubscriptionState::kNotSubscribed;
    auto subscription_state_callback = [&last_subscription_state](SubscriptionState new_state) -> bool {
        last_subscription_state = new_state;
        return true;
    };
    std::ignore = this->test_proxy_event_->SetSubscriptionStateChangeHandler(subscription_state_callback);

    // When subscribed
    std::ignore = this->test_proxy_event_->Subscribe(1U);

    // Then the callback is triggered with kSubscribed new status
    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kSubscribed);
    EXPECT_EQ(last_subscription_state, SubscriptionState::kSubscribed);

    // and when unsubscribed
    this->test_proxy_event_->Unsubscribe();

    // Then the callback is triggered with kNotSubscribed new status
    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kNotSubscribed);
    EXPECT_EQ(last_subscription_state, SubscriptionState::kNotSubscribed);
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, RegisterSubscriptionStateChangeHandlerSelfRemoving)
{
    // Given a valid proxy with state change callback (self-removing at subscription) that is not subscribed yet
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    SubscriptionState last_subscription_state = SubscriptionState::kNotSubscribed;
    auto subscription_state_callback = [&last_subscription_state](SubscriptionState new_state) -> bool {
        last_subscription_state = new_state;
        return new_state != SubscriptionState::kSubscribed;
    };
    std::ignore = this->test_proxy_event_->SetSubscriptionStateChangeHandler(subscription_state_callback);

    // When subscribed
    std::ignore = this->test_proxy_event_->Subscribe(1U);

    // Then the callback is triggered with kSubscribed new status
    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kSubscribed);
    EXPECT_EQ(last_subscription_state, SubscriptionState::kSubscribed);

    // and when unsubscribed
    this->test_proxy_event_->Unsubscribe();

    // Then the callback is not triggered with the new status
    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kNotSubscribed);
    EXPECT_EQ(last_subscription_state, SubscriptionState::kSubscribed);
}

TEST_F(LolaProxyEventSubscriptionScopeFixture, RegisterAndRemoveSubscriptionStateChangeHandler)
{
    // Given a valid proxy with state change callback that is not subscribed yet
    this->GivenAProxyEvent(this->element_fq_id_, this->event_name_);
    SubscriptionState last_subscription_state = SubscriptionState::kNotSubscribed;
    auto subscription_state_callback = [&last_subscription_state](SubscriptionState new_state) -> bool {
        last_subscription_state = new_state;
        return true;
    };
    std::ignore = this->test_proxy_event_->SetSubscriptionStateChangeHandler(subscription_state_callback);

    // When removing the callback and subscribing
    std::ignore = this->test_proxy_event_->UnsetSubscriptionStateChangeHandler();
    std::ignore = this->test_proxy_event_->Subscribe(1U);

    // Then the callback is not triggered
    EXPECT_EQ(this->test_proxy_event_->GetSubscriptionState(), SubscriptionState::kSubscribed);
    EXPECT_EQ(last_subscription_state, SubscriptionState::kNotSubscribed);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
