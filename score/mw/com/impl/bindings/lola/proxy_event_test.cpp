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
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_fixture.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/service_element_type.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <type_traits>

namespace score::mw::com::impl::lola
{
namespace
{

using LolaProxyEventConstructionFixture = LolaProxyEventFixture;
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

TEST_F(LolaProxyEventDeathTest, FailOnEventNotFound)
{
    const ElementFqId bad_element_fq_id{0xcdef, 0x6, 0x10, ServiceElementType::EVENT};
    const std::string bad_event_name{"BadEventName"};

    EXPECT_DEATH(score::cpp::ignore = this->GivenAProxyEvent(bad_element_fq_id, bad_event_name), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
