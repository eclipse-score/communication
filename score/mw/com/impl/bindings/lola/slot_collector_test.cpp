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
#include "score/mw/com/impl/bindings/lola/slot_collector.h"

#include "score/mw/com/impl/bindings/lola/proxy_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{

namespace
{

constexpr std::size_t kMaxSubscribers{5U};
const TransactionLogId kDummyTransactionLogId{10U};

using namespace ::score::memory::shared;
class SlotCollectorWithFakeMem : public ::testing::Test
{
  protected:
    SlotIndexType AllocateSlot(const EventSlotStatus::EventTimeStamp timestamp = 1)
    {
        const auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
        EXPECT_TRUE(slot.IsValid());
        skeleton_event_data_control_local_.EventReady(slot, timestamp);
        return slot.GetIndex();
    }

    std::size_t CalculateNumberOfCollectedSlots(const SlotCollector::SlotIndicators& indices)
    {
        return static_cast<std::size_t>(std::distance(indices.begin, indices.end));
    }

    FakeMemoryResource fake_memory_resource_;
    EventDataControl event_data_control_{5, fake_memory_resource_, kMaxSubscribers};
    ProxyEventDataControlLocalView<> proxy_event_data_control_local_{event_data_control_};
    SkeletonEventDataControlLocalView<> skeleton_event_data_control_local_{event_data_control_};
    TransactionLogSet::TransactionLogIndex transaction_log_index_{
        proxy_event_data_control_local_.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value()};
};

TEST_F(SlotCollectorWithFakeMem, TestProperEventAcquisition)
{
    AllocateSlot();
    SlotCollector slot_collector{proxy_event_data_control_local_, 1U, transaction_log_index_};
    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 1);

    const std::size_t max_count{1};
    const auto slot_indices = slot_collector.GetNewSamplesSlotIndices(max_count);

    EXPECT_EQ(CalculateNumberOfCollectedSlots(slot_indices), 1);
    EXPECT_EQ(slot_indices.begin->GetIndex(), 0);
}

TEST_F(SlotCollectorWithFakeMem, ReceiveEventsInOrder)
{
    EventSlotStatus::EventTimeStamp send_time{1};
    const std::size_t num_values_to_send{3};
    for (std::size_t i = 0; i < num_values_to_send; ++i)
    {
        AllocateSlot(send_time);
        send_time++;
    }

    SlotCollector slot_collector{proxy_event_data_control_local_, 3U, transaction_log_index_};
    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 3);

    const std::size_t max_count{3};
    const auto slot_indices = slot_collector.GetNewSamplesSlotIndices(max_count);

    EXPECT_EQ(CalculateNumberOfCollectedSlots(slot_indices), 3);

    SlotIndexType current_slot_index = 0;
    for (auto it = slot_indices.begin; it != slot_indices.end; ++it)
    {
        EXPECT_EQ(it->GetIndex(), current_slot_index);
        current_slot_index++;
    }

    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 0);
    const std::size_t new_max_count{15};
    const auto no_new_sample = slot_collector.GetNewSamplesSlotIndices(new_max_count);
    EXPECT_EQ(CalculateNumberOfCollectedSlots(no_new_sample), 0);
}

TEST_F(SlotCollectorWithFakeMem, DoNotReceiveEventsFromThePast)
{
    SlotCollector slot_collector{proxy_event_data_control_local_, 2U, transaction_log_index_};

    AllocateSlot(17);
    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 1);

    const std::size_t max_count{37};
    const auto slot_indices = slot_collector.GetNewSamplesSlotIndices(max_count);

    EXPECT_EQ(CalculateNumberOfCollectedSlots(slot_indices), 1);

    AllocateSlot(1);

    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 0);
    const std::size_t new_max_count{38};
    const auto no_new_sample = slot_collector.GetNewSamplesSlotIndices(new_max_count);
    EXPECT_EQ(CalculateNumberOfCollectedSlots(no_new_sample), 0);
}

using SlotCollectorWithFakeMemDeathTest = SlotCollectorWithFakeMem;
TEST_F(SlotCollectorWithFakeMemDeathTest, CreatingSlotCollectorWith0MaxSlotsTerminates)
{
    // Given an EventDataControl and registered TransactionLog

    // When creating a SlotCollector with max_slots of 0
    // Then the program terminates
    EXPECT_DEATH(SlotCollector(proxy_event_data_control_local_, 0U, transaction_log_index_), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
