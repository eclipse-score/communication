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

#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_coherent.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed.h"
#include "score/mw/com/impl/bindings/lola/event_data_control_mixed_composite.h"
#include "score/mw/com/impl/bindings/lola/provider_event_data_control_coherent_local_view.h"
#include "score/mw/com/impl/bindings/lola/provider_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{

namespace
{

constexpr std::size_t kMaxSlots{5U};

using namespace ::score::memory::shared;
class SlotCollectorWithFakeMem : public ::testing::Test
{
  protected:
    SlotIndexType AllocateSlot(const EventSlotStatus::EventTimeStamp timestamp = 1)
    {
        const auto allocated_slot = provider_event_data_control_local_.AllocateNextSlot();
        EXPECT_TRUE(allocated_slot.has_value());
        provider_event_data_control_local_.EventReady(allocated_slot.value(), timestamp);
        return allocated_slot.value();
    }

    std::size_t CalculateNumberOfCollectedSlots(const SlotCollector::SlotIndices& indices)
    {
        return static_cast<std::size_t>(std::distance(indices.begin, indices.end));
    }

    FakeMemoryResource fake_memory_resource_;
    EventDataControl event_data_control_{kMaxSlots, fake_memory_resource_};
    TransactionLog transaction_log_{kMaxSlots, fake_memory_resource_};
    ConsumerEventDataControlLocalView<> consumer_event_data_control_local_{event_data_control_, transaction_log_};
    ProviderEventDataControlLocalView<> provider_event_data_control_local_{event_data_control_};
};

TEST_F(SlotCollectorWithFakeMem, TestProperEventAcquisition)
{
    AllocateSlot();
    SlotCollector slot_collector{consumer_event_data_control_local_, 1U};
    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 1);

    const std::size_t max_count{1};
    const auto slot_indices = slot_collector.GetNewSamplesSlotIndices(max_count);

    EXPECT_EQ(CalculateNumberOfCollectedSlots(slot_indices), 1);
    EXPECT_EQ(*slot_indices.begin, 0);
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

    SlotCollector slot_collector{consumer_event_data_control_local_, 3U};
    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 3);

    const std::size_t max_count{3};
    const auto slot_indices = slot_collector.GetNewSamplesSlotIndices(max_count);

    EXPECT_EQ(CalculateNumberOfCollectedSlots(slot_indices), 3);

    SlotIndexType current_slot_index = 0;
    for (auto it = slot_indices.begin; it != slot_indices.end; ++it)
    {
        EXPECT_EQ(*it, current_slot_index);
        current_slot_index++;
    }

    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 0);
    const std::size_t new_max_count{15};
    const auto no_new_sample = slot_collector.GetNewSamplesSlotIndices(new_max_count);
    EXPECT_EQ(CalculateNumberOfCollectedSlots(no_new_sample), 0);
}

TEST_F(SlotCollectorWithFakeMem, DoNotReceiveEventsFromThePast)
{
    SlotCollector slot_collector{consumer_event_data_control_local_, 2U};

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
    EXPECT_DEATH(SlotCollector(consumer_event_data_control_local_, 0U), ".*");
}

class SlotCollectorCoherentWithFakeMem : public ::testing::Test
{
  protected:
    SlotIndexType AllocateSlot(const CoherentEventSlotStatus::EventTimeStamp timestamp = 1)
    {
        const auto allocated_slot = provider_event_data_control_local_.AllocateNextSlot();
        provider_event_data_control_local_.EventReady(allocated_slot, timestamp);
        return allocated_slot;
    }

    std::size_t CalculateNumberOfCollectedSlots(const SlotCollector::SlotIndices& indices)
    {
        return static_cast<std::size_t>(std::distance(indices.begin, indices.end));
    }

    FakeMemoryResource fake_memory_resource_;
    EventDataControlCoherent event_data_control_{kMaxSlots, fake_memory_resource_};
    ConsumerEventDataControlCoherentLocalView consumer_event_data_control_local_{event_data_control_};
    ProviderEventDataControlCoherentLocalView provider_event_data_control_local_{event_data_control_};
};

TEST_F(SlotCollectorCoherentWithFakeMem, TestProperEventAcquisition)
{
    AllocateSlot();
    SlotCollector slot_collector{consumer_event_data_control_local_, 1U};
    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 1);

    const std::size_t max_count{1};
    const auto slot_indices = slot_collector.GetNewSamplesSlotIndices(max_count);

    EXPECT_EQ(CalculateNumberOfCollectedSlots(slot_indices), 1);
    EXPECT_EQ(*slot_indices.begin, 0);
}

TEST_F(SlotCollectorCoherentWithFakeMem, ReceiveEventsInOrder)
{
    CoherentEventSlotStatus::EventTimeStamp send_time{1};
    const std::size_t num_values_to_send{3};
    for (std::size_t i = 0; i < num_values_to_send; ++i)
    {
        AllocateSlot(send_time);
        send_time++;
    }

    SlotCollector slot_collector{consumer_event_data_control_local_, 3U};
    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 3);

    const std::size_t max_count{3};
    const auto slot_indices = slot_collector.GetNewSamplesSlotIndices(max_count);

    EXPECT_EQ(CalculateNumberOfCollectedSlots(slot_indices), 3);

    SlotIndexType current_slot_index = 0;
    for (auto it = slot_indices.begin; it != slot_indices.end; ++it)
    {
        EXPECT_EQ(*it, current_slot_index);
        current_slot_index++;
    }

    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 0);
    const std::size_t new_max_count{15};
    const auto no_new_sample = slot_collector.GetNewSamplesSlotIndices(new_max_count);
    EXPECT_EQ(CalculateNumberOfCollectedSlots(no_new_sample), 0);
}

TEST_F(SlotCollectorCoherentWithFakeMem, DoNotReceiveEventsFromThePast)
{
    SlotCollector slot_collector{consumer_event_data_control_local_, 2U};

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

using SlotCollectorCoherentWithFakeMemDeathTest = SlotCollectorCoherentWithFakeMem;
TEST_F(SlotCollectorCoherentWithFakeMemDeathTest, CreatingSlotCollectorWith0MaxSlotsTerminates)
{
    // Given an EventDataControlCoherent

    // When creating a SlotCollector with max_slots of 0
    // Then the program terminates
    EXPECT_DEATH(SlotCollector(consumer_event_data_control_local_, 0U), ".*");
}

class SlotCollectorMixedCoherentWithFakeMem : public ::testing::Test
{
  protected:
    SlotIndexType AllocateSlot(const EventSlotStatus::EventTimeStamp timestamp = 1)
    {
        const auto allocation_result = provider_event_data_control_local_.GetComposite().AllocateNextSlot();
        EXPECT_TRUE(allocation_result.allocated_slot_index.has_value());
        const auto slot_index = allocation_result.allocated_slot_index.value();
        provider_event_data_control_local_.EventReady(slot_index, timestamp);
        return slot_index;
    }

    std::size_t CalculateNumberOfCollectedSlots(const SlotCollector::SlotIndices& indices)
    {
        return static_cast<std::size_t>(std::distance(indices.begin, indices.end));
    }

    FakeMemoryResource fake_memory_resource_;
    EventDataControlMixed event_data_control_mixed_{kMaxSlots, fake_memory_resource_};
    EventDataControlMixedComposite provider_event_data_control_local_{event_data_control_mixed_};
    ConsumerEventDataControlMixedCoherentLocalView consumer_event_data_control_local_{event_data_control_mixed_};
};

TEST_F(SlotCollectorMixedCoherentWithFakeMem, TestProperEventAcquisition)
{
    AllocateSlot();
    SlotCollector slot_collector{consumer_event_data_control_local_, 1U};
    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 1);

    const std::size_t max_count{1};
    const auto slot_indices = slot_collector.GetNewSamplesSlotIndices(max_count);

    EXPECT_EQ(CalculateNumberOfCollectedSlots(slot_indices), 1);
    EXPECT_EQ(*slot_indices.begin, 0);
}

TEST_F(SlotCollectorMixedCoherentWithFakeMem, ReceiveEventsInOrder)
{
    EventSlotStatus::EventTimeStamp send_time{1};
    const std::size_t num_values_to_send{3};
    for (std::size_t i = 0; i < num_values_to_send; ++i)
    {
        AllocateSlot(send_time);
        send_time++;
    }

    SlotCollector slot_collector{consumer_event_data_control_local_, 3U};
    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 3);

    const std::size_t max_count{3};
    const auto slot_indices = slot_collector.GetNewSamplesSlotIndices(max_count);

    EXPECT_EQ(CalculateNumberOfCollectedSlots(slot_indices), 3);

    SlotIndexType current_slot_index = 0;
    for (auto it = slot_indices.begin; it != slot_indices.end; ++it)
    {
        EXPECT_EQ(*it, current_slot_index);
        current_slot_index++;
    }

    EXPECT_EQ(slot_collector.GetNumNewSamplesAvailable(), 0);
    const std::size_t new_max_count{15};
    const auto no_new_sample = slot_collector.GetNewSamplesSlotIndices(new_max_count);
    EXPECT_EQ(CalculateNumberOfCollectedSlots(no_new_sample), 0);
}

TEST_F(SlotCollectorMixedCoherentWithFakeMem, DoNotReceiveEventsFromThePast)
{
    SlotCollector slot_collector{consumer_event_data_control_local_, 2U};

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

using SlotCollectorMixedCoherentWithFakeMemDeathTest = SlotCollectorMixedCoherentWithFakeMem;
TEST_F(SlotCollectorMixedCoherentWithFakeMemDeathTest, CreatingSlotCollectorWith0MaxSlotsTerminates)
{
    // Given an EventDataControlMixed

    // When creating a SlotCollector with max_slots of 0
    // Then the program terminates
    EXPECT_DEATH(SlotCollector(consumer_event_data_control_local_, 0U), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
