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
#include "score/mw/com/impl/bindings/someip/in_process_transport.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace score::mw::com::impl::someip
{
namespace
{

const ElementFqId kEventFqId{1U, 2U, 3U, ServiceElementType::EVENT};
const ElementFqId kOtherEventFqId{1U, 4U, 3U, ServiceElementType::EVENT};

class InProcessTransportFixture : public ::testing::Test
{
  protected:
    InProcessTransport unit_{};
};

TEST_F(InProcessTransportFixture, SendingToAnEventWhichIsNotOfferedFails)
{
    const std::uint32_t payload{42U};

    const auto result = unit_.SendEvent(kEventFqId, &payload, sizeof(payload));

    ASSERT_FALSE(result.has_value());
}

TEST_F(InProcessTransportFixture, SentPayloadCanBeRetrieved)
{
    ASSERT_TRUE(unit_.OfferEvent(kEventFqId).has_value());
    const std::array<std::uint8_t, 3U> payload{1U, 2U, 3U};

    ASSERT_TRUE(unit_.SendEvent(kEventFqId, payload.data(), payload.size()).has_value());

    const auto received_payload = unit_.GetLastSentPayload(kEventFqId);
    ASSERT_EQ(received_payload.size(), payload.size());
    for (std::size_t index = 0U; index < payload.size(); ++index)
    {
        EXPECT_EQ(static_cast<std::uint8_t>(received_payload[index]), payload[index]);
    }
}

TEST_F(InProcessTransportFixture, SendingDoesNotAffectOtherEvents)
{
    ASSERT_TRUE(unit_.OfferEvent(kEventFqId).has_value());
    ASSERT_TRUE(unit_.OfferEvent(kOtherEventFqId).has_value());
    const std::uint32_t payload{42U};

    ASSERT_TRUE(unit_.SendEvent(kEventFqId, &payload, sizeof(payload)).has_value());

    EXPECT_EQ(unit_.GetLastSentPayload(kEventFqId).size(), sizeof(payload));
    EXPECT_TRUE(unit_.GetLastSentPayload(kOtherEventFqId).empty());
}

TEST_F(InProcessTransportFixture, StopOfferingRemovesTheEvent)
{
    ASSERT_TRUE(unit_.OfferEvent(kEventFqId).has_value());

    unit_.StopOfferEvent(kEventFqId);

    const std::uint32_t payload{42U};
    EXPECT_FALSE(unit_.SendEvent(kEventFqId, &payload, sizeof(payload)).has_value());
}

TEST_F(InProcessTransportFixture, NotifyingAnEventWhichIsNotOfferedFails)
{
    EXPECT_FALSE(unit_.NotifyEvent(kEventFqId).has_value());
}

TEST_F(InProcessTransportFixture, NotificationsAreCounted)
{
    ASSERT_TRUE(unit_.OfferEvent(kEventFqId).has_value());

    ASSERT_TRUE(unit_.NotifyEvent(kEventFqId).has_value());
    ASSERT_TRUE(unit_.NotifyEvent(kEventFqId).has_value());

    EXPECT_EQ(unit_.GetNotificationCount(kEventFqId), 2U);
}

TEST_F(InProcessTransportFixture, HasSubscribersReflectsSubscriptions)
{
    ASSERT_TRUE(unit_.OfferEvent(kEventFqId).has_value());
    EXPECT_FALSE(unit_.HasSubscribers(kEventFqId));

    const auto subscription_id = unit_.Subscribe(kEventFqId);
    ASSERT_TRUE(subscription_id.has_value());
    EXPECT_TRUE(unit_.HasSubscribers(kEventFqId));

    unit_.Unsubscribe(kEventFqId, subscription_id.value());
    EXPECT_FALSE(unit_.HasSubscribers(kEventFqId));
}

TEST_F(InProcessTransportFixture, SubscribingToAnEventWhichIsNotOfferedFails)
{
    EXPECT_FALSE(unit_.Subscribe(kEventFqId).has_value());
}

TEST_F(InProcessTransportFixture, ResetDropsAllState)
{
    ASSERT_TRUE(unit_.OfferEvent(kEventFqId).has_value());
    const std::uint32_t payload{42U};
    ASSERT_TRUE(unit_.SendEvent(kEventFqId, &payload, sizeof(payload)).has_value());

    unit_.Reset();

    EXPECT_TRUE(unit_.GetLastSentPayload(kEventFqId).empty());
    EXPECT_FALSE(unit_.SendEvent(kEventFqId, &payload, sizeof(payload)).has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::someip
