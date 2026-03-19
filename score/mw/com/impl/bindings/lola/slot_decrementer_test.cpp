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
#include "score/mw/com/impl/bindings/lola/slot_decrementer.h"

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/proxy_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_id.h"

#include <score/optional.hpp>

#include <gtest/gtest.h>
#include <cstddef>
#include <utility>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr std::size_t kMaxSlots{5U};
constexpr std::size_t kMaxSubscribers{5U};
const TransactionLogId kDummyTransactionLogId{10U};

class SlotDecrementerFixture : public ::testing::Test
{
  public:
    SlotDecrementerFixture& WithASlotDecrementer()
    {
        const auto slot_index = AllocateSlotAndReferenceEvent();
        score::cpp::ignore = event_slot_index_.emplace(slot_index);
        slot_decrementer_ = SlotDecrementer{proxy_event_data_control_local_, slot_index, transaction_log_index_};
        return *this;
    }

    SlotIndexType AllocateSlotAndReferenceEvent(EventSlotStatus::EventTimeStamp timestamp = 1)
    {
        // Allocate a slot which will acquire it for writing
        auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
        EXPECT_TRUE(slot.has_value());

        // Mark the slot as ready which allows it to be read
        skeleton_event_data_control_local_.EventReady(slot.value(), timestamp);

        // Reference the slot which indicates that a consumer is currently reading it. Use timestamp - 1 to ensure that
        // we find the slot that we just marked as ready first.
        auto client_slot_result =
            proxy_event_data_control_local_.ReferenceNextEvent(timestamp - 1, transaction_log_index_);
        EXPECT_TRUE(client_slot_result.has_value());

        return client_slot_result.value();
    }

    EventSlotStatus::SubscriberCount GetSlotReferenceCount(const SlotIndexType event_slot_index)
    {
        return EventSlotStatus{skeleton_event_data_control_local_[event_slot_index]}.GetReferenceCount();
    }

    FakeMemoryResource memory_{};
    EventDataControl event_data_control_{kMaxSlots, memory_, kMaxSubscribers};
    ProxyEventDataControlLocalView<> proxy_event_data_control_local_{event_data_control_};
    SkeletonEventDataControlLocalView<> skeleton_event_data_control_local_{event_data_control_};
    TransactionLogSet::TransactionLogIndex transaction_log_index_{
        proxy_event_data_control_local_.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value()};

    std::optional<SlotIndexType> event_slot_index_{};
    score::cpp::optional<SlotDecrementer> slot_decrementer_{};
};

TEST_F(SlotDecrementerFixture, CreatingSlotDecrementerWithReferencedSlotMaintainsReferenceCount)
{
    // When creating a SlotDecrementer from a reference slot in EventDataControl
    WithASlotDecrementer();

    // Then the reference count of the slot managed by the SlotDecrementer will be 1
    EXPECT_EQ(GetSlotReferenceCount(event_slot_index_.value()), 1U);
}

TEST_F(SlotDecrementerFixture, DestroyingSlotDecrementerDereferencesSlot)
{
    // Given a SlotDecrementer
    WithASlotDecrementer();

    // When destroying the SlotDecrementer
    slot_decrementer_.reset();

    // Then the reference count of the slot managed by the SlotDecrementer will be decremented
    EXPECT_EQ(GetSlotReferenceCount(event_slot_index_.value()), 0U);
}

TEST_F(SlotDecrementerFixture, MoveConstructingWillNotDereferenceSlot)
{
    // Given a SlotDecrementer
    WithASlotDecrementer();

    // When move constructing the SlotDecrementer
    const auto new_slot_decrementer{std::move(slot_decrementer_.value())};

    // Then the reference count of the slot managed by the SlotDecrementer will still be 1
    EXPECT_EQ(GetSlotReferenceCount(event_slot_index_.value()), 1U);
}

TEST_F(SlotDecrementerFixture, MoveAssigningWillDecrementSlotOfMovedToSlotDecrementer)
{
    // Given a SlotDecrementer
    WithASlotDecrementer();

    // and a second SlotDecrementer referencing a different slot
    const auto second_slot_index = AllocateSlotAndReferenceEvent(2);
    SlotDecrementer second_slot_decrementer{proxy_event_data_control_local_, second_slot_index, transaction_log_index_};

    // When move assigning the second SlotDecrementer to the first
    slot_decrementer_.value() = std::move(second_slot_decrementer);

    // Then the reference count of the moved to slot will be decremented
    EXPECT_EQ(GetSlotReferenceCount(event_slot_index_.value()), 0U);
    // and the reference count of the moved from slot will not be decremented
    EXPECT_EQ(GetSlotReferenceCount(second_slot_index), 1U);
}

TEST_F(SlotDecrementerFixture, SelfMoveAssigningWillNotDereferenceSlot)
{
    // Given a SlotDecrementer
    WithASlotDecrementer();

    // When move assigning the SlotDecrementer to itself
    slot_decrementer_.value() = std::move(slot_decrementer_.value());

    // Then the reference count of the slot managed by the SlotDecrementer will still be 1
    EXPECT_EQ(GetSlotReferenceCount(event_slot_index_.value()), 1U);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
