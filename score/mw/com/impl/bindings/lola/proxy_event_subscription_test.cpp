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
#include "score/mw/com/impl/subscription_state.h"

#include "score/language/safecpp/scoped_function/scope.h"

#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <tuple>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::_;

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
