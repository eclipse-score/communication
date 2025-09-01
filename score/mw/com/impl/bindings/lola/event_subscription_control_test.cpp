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
#include "score/mw/com/impl/bindings/lola/event_subscription_control.h"

#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"

#include "score/memory/shared/atomic_indirector.h"
#include "score/memory/shared/atomic_mock.h"

#include <gtest/gtest.h>

#include <random>
#include <thread>

namespace score::mw::com::impl::lola
{

namespace
{

TEST(EventSubscriptionControl, Create)
{
    // Expect, that we can create EventSubscriptionControl with real and mocked AtomicIndirector types without crash
    EventSubscriptionControl unit1{20, 3, true};
    detail_event_subscription_control::EventSubscriptionControlImpl<score::memory::shared::AtomicIndirectorMock> unit2{
        20, 3, true};
}

TEST(EventSubscriptionControl, Subscribe_OK)
{
    // Given a unit with a given slot count and max subscribers
    EventSubscriptionControl unit{20, 3, true};

    // expect, that we can do multiple Subscribe() calls successfully as long as we are within slot_count/maxSubscriber
    // bounds
    EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);
    EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);
    EXPECT_EQ(unit.Subscribe(10), SubscribeResult::kSuccess);
}

TEST(EventSubscriptionControl, Subscribe_Failed_Slots)
{
    // Given a unit with a given slot count and max subscribers
    EventSubscriptionControl unit{20, 3, true};

    // expect, that we can do Subscribe() calls successfully as long as we are within slot_count/maxSubscriber
    // bounds
    EXPECT_EQ(unit.Subscribe(20), SubscribeResult::kSuccess);
    // but expect, that an additional subscribe call overflowing slot count would fail
    EXPECT_EQ(unit.Subscribe(1), SubscribeResult::kSlotOverflow);
}

TEST(EventSubscriptionControl, Subscribe_NotEnforceMaxSamples)
{
    // Given a unit with a given slot count and max subscribers, which doesn't force max samples
    EventSubscriptionControl unit{20, 3, false};

    // expect, that we can do Subscribe() calls successfully as long as we are within slot_count/maxSubscriber
    // bounds
    EXPECT_EQ(unit.Subscribe(20), SubscribeResult::kSuccess);
    // and expect, that an additional subscribe call overflowing slot count is successful
    EXPECT_EQ(unit.Subscribe(1), SubscribeResult::kSuccess);
}

TEST(EventSubscriptionControl, Subscribe_Failed_Subscribers)
{
    // Given a unit with a given slot count and max subscribers
    EventSubscriptionControl unit{20, 3, true};

    // expect, that we can do Subscribe() calls successfully as long as we are within slot_count/maxSubscriber
    // bounds
    EXPECT_EQ(unit.Subscribe(10), SubscribeResult::kSuccess);
    EXPECT_EQ(unit.Subscribe(1), SubscribeResult::kSuccess);
    EXPECT_EQ(unit.Subscribe(1), SubscribeResult::kSuccess);

    // but expect, that an additional subscribe call overflowing max subscribers would fail
    EXPECT_EQ(unit.Subscribe(1), SubscribeResult::kMaxSubscribersOverflow);
}

TEST(EventSubscriptionControl, Subscribe_Unsubscribe_Slots_OK)
{
    // Given a unit with a given slot count and max subscribers
    EventSubscriptionControl unit{20, 3, true};

    // expect, that we can do multiple Subscribe() calls successfully as long as we are within slot_count/maxSubscriber
    // bounds
    EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);
    EXPECT_EQ(unit.Subscribe(15), SubscribeResult::kSuccess);

    // and if we Unsubscribe again, we can subscribe again
    unit.Unsubscribe(5);
    EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);
}

TEST(EventSubscriptionControl, Subscribe_Unsubscribe_Subscribers_OK)
{
    // Given a unit with a given slot count and max subscribers
    EventSubscriptionControl unit{20, 3, true};

    // expect, that we can do multiple Subscribe() calls successfully as long as we are within slot_count/maxSubscriber
    // bounds
    EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);
    EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);
    EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);

    // and if we Unsubscribe again, we can subscribe again
    unit.Unsubscribe(5);
    EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);
}

TEST(EventSubscriptionControl, ConcurrentAccess)
{
    //  Given a unit with a given slot count and max subscribers
    EventSubscriptionControl unit{20, 3, true};

    auto thread_action = [&unit]() -> void {
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> dist10_50(10, 50);
        using namespace std::chrono_literals;
        for (auto i = 0; i < 100; ++i)
        {
            auto result = unit.Subscribe(5);
            if (result == SubscribeResult::kSuccess)
            {
                std::this_thread::sleep_for(dist10_50(rng) * 1ms);
                unit.Unsubscribe(5);
            }
            else
            {
                // if subscribe fails, it can only be the case of a compare_exchange retry failure!
                ASSERT_EQ(result, SubscribeResult::kUpdateRetryFailure);
            }
        }
    };

    std::thread t1(thread_action);
    std::thread t2(thread_action);
    std::thread t3(thread_action);
    t1.join();
    t2.join();
    t3.join();
}

TEST(EventSubscriptionControl, CompareExchangeBehaviour_Subscribe)
{
    using namespace score::memory::shared;
    using ::testing::_;
    using ::testing::Return;

    AtomicMock<std::uint32_t> atomic_mock;
    AtomicIndirectorMock<std::uint32_t>::SetMockObject(&atomic_mock);

    EventSubscriptionControl::SubscriberCountType max_subscribers{3};
    auto num_retries = 2 * max_subscribers;

    // Given the operation to update state fails num_retries times
    EXPECT_CALL(atomic_mock, compare_exchange_weak(_, _, _)).Times(num_retries).WillRepeatedly(Return(false));

    detail_event_subscription_control::EventSubscriptionControlImpl<score::memory::shared::AtomicIndirectorMock> unit{
        20, 3, true};

    // when calling subscribe()
    EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kUpdateRetryFailure);
}

using EventSubscriptionControlDeathTest = EventSubscriptionControl;
TEST(EventSubscriptionControlDeathTest, CompareExchangeBehaviour_Unsubscribe_RetryLimit)
{
    using namespace score::memory::shared;
    using ::testing::_;
    using ::testing::Return;

    auto unsubscribe_hits_retry_limit = []() -> void {
        AtomicMock<std::uint32_t> atomic_mock;
        AtomicIndirectorMock<std::uint32_t>::SetMockObject(&atomic_mock);

        EventSubscriptionControl::SubscriberCountType max_subscribers{3};
        auto num_retries = 2 * max_subscribers;
        detail_event_subscription_control::EventSubscriptionControlImpl<score::memory::shared::AtomicIndirectorMock> unit{
            20, 3, true};
        EventSubscriptionControlAttorney<
            detail_event_subscription_control::EventSubscriptionControlImpl<score::memory::shared::AtomicIndirectorMock>>
            attorney{unit};

        // Given the unit has currently one subscriber and 5 subscribed slots
        std::uint32_t state{1U};
        state = state << 16U;
        state += 5;
        attorney.SetCurrentState(state);

        // Given the operation to update the state fails num_retries times for Unsubscribe
        EXPECT_CALL(atomic_mock, compare_exchange_weak(_, _, _)).Times(num_retries).WillRepeatedly(Return(false));

        // when calling Unsubscribe()
        unit.Unsubscribe(5);
    };
    // then expect, that we die.
    EXPECT_DEATH(unsubscribe_hits_retry_limit(), ".*");
}

TEST(EventSubscriptionControlDeathTest, Unsubscribe_SubscriberUnderflow_Dies)
{
    auto unsubscribe_without_subscribe = []() -> void {
        //  Given a unit with a given slot count and max subscribers
        EventSubscriptionControl unit{20, 3, true};
        // given we have already one successful subscribe
        EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);
        // If we unsubscribe twice
        unit.Unsubscribe(2);
        unit.Unsubscribe(2);
    };
    // then expect, that we die.
    EXPECT_DEATH(unsubscribe_without_subscribe(), ".*");
}

TEST(EventSubscriptionControlDeathTest, Unsubscribe_SlotUnderflow_Dies)
{
    auto unsubscribe_with_slot_underflow = []() -> void {
        //  Given a unit with a given slot count and max subscribers
        EventSubscriptionControl unit{20, 3, true};
        // given we have already one successful subscribe
        EXPECT_EQ(unit.Subscribe(5), SubscribeResult::kSuccess);
        // and if we unsubscribe, with a higher number of slot than subscribed
        unit.Unsubscribe(6);
    };
    // then expect, that we die.
    EXPECT_DEATH(unsubscribe_with_slot_underflow(), ".*");
}

TEST(EventSubscriptionControl, ToStringShouldReturnExpectedStringForAllSubscribeResultTypes)
{
    // When converting each enum value to string
    // Then the result should be the expected string
    EXPECT_EQ(ToString(SubscribeResult::kSuccess), "success");
    EXPECT_EQ(ToString(SubscribeResult::kMaxSubscribersOverflow), "Max subcribers overflow");
    EXPECT_EQ(ToString(SubscribeResult::kSlotOverflow), "Slot overflow");
    EXPECT_EQ(ToString(SubscribeResult::kUpdateRetryFailure), "Update retry failure");
    EXPECT_EQ(ToString(static_cast<SubscribeResult>(255U)), "Unknown SubscribeResult value");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
