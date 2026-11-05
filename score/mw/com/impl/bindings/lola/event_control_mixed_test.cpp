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
#include "score/mw/com/impl/bindings/lola/event_control_mixed.h"

#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

TEST(EventControlMixedTest, ConstructionInitializesDataControlAndSubscriptionControl)
{
    FakeMemoryResource memory{};
    constexpr SlotIndexType kNumberOfSlots{5U};
    constexpr EventControlMixed::SubscriberCountType kMaxSubscribers{3U};

    const EventControlMixed unit{kNumberOfSlots, kMaxSubscribers, true, memory};

    EXPECT_EQ(unit.data_control.data_control_.state_slots_.size(), kNumberOfSlots);
    EXPECT_FALSE(unit.data_control.newest_index_.has_value());
}

TEST(EventControlMixedTest, TransactionLogSetSupportsRegistrationUpToMaxSubscribers)
{
    // Given an EventControlMixed, constructed with a given max_subscribers
    FakeMemoryResource memory{};
    constexpr SlotIndexType kNumberOfSlots{5U};
    constexpr EventControlMixed::SubscriberCountType kMaxSubscribers{2U};
    EventControlMixed unit{kNumberOfSlots, kMaxSubscribers, true, memory};

    // Then its transaction_log_set_ can be used to register up to max_subscribers proxy elements (as for
    // EventControl, since CoherencyType::kMixed events' non-coherent consumers require the same per-slot
    // transaction-log based crash-recovery)
    auto registration_result_1 = unit.transaction_log_set_.RegisterProxyElement(TransactionLogId{1U});
    ASSERT_TRUE(registration_result_1.has_value());
    auto registration_result_2 = unit.transaction_log_set_.RegisterProxyElement(TransactionLogId{2U});
    ASSERT_TRUE(registration_result_2.has_value());

    // and registering one more than max_subscribers fails
    const auto registration_result_3 = unit.transaction_log_set_.RegisterProxyElement(TransactionLogId{3U});
    EXPECT_FALSE(registration_result_3.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
