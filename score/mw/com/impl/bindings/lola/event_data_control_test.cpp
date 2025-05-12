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
#include "score/mw/com/impl/bindings/lola/event_data_control.h"

#include "score/containers/dynamic_array.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"
#include "score/mw/com/impl/instance_specifier.h"

#include "score/memory/shared/atomic_indirector.h"
#include "score/memory/shared/atomic_mock.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>
#include <chrono>
#include <limits>
#include <mutex>
#include <random>
#include <set>
#include <thread>

namespace score::mw::com::impl::lola
{

namespace
{

using ::testing::_;
using ::testing::Return;

constexpr std::size_t kMaxSlots{5U};
constexpr std::size_t kMaxSubscribers{5U};
const TransactionLogId kDummyTransactionLogId{10U};

constexpr auto kSlotIsInWriting = std::numeric_limits<EventSlotStatus::SubscriberCount>::max();

std::size_t RandomNumberBetween(std::size_t lower, std::size_t upper)
{
    std::mt19937_64 random_number_generator{};
    auto time_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::seed_seq sequence_seed{uint32_t(time_seed & 0xffffffff), std::uint32_t(time_seed >> 32)};
    random_number_generator.seed(sequence_seed);
    std::uniform_int_distribution<std::size_t> number_distribution(lower, upper);
    return number_distribution(random_number_generator);
}

bool RandomTrueOrFalse()
{
    return RandomNumberBetween(0, 1);
}

TEST(EventDataControlTest, EventDataControlUsesADynamicArrayToRepresentSlots)
{
    // Our detailed design (aas/lib/memory/design/shared_memory/OffsetPtrDesign.md#dynamic-array-considerations)
    // requires that we use a DynamicArray to represent our slots so that bounds checking is done.
    static_assert(
        std::is_same_v<score::containers::DynamicArray<
                           std::atomic<EventSlotStatus::value_type>,
                           memory::shared::PolymorphicOffsetPtrAllocator<std::atomic<EventSlotStatus::value_type>>>,
                       EventDataControl::EventControlSlots>,
        "EventDataControl should use a dynamic array to represent slots.");
}

class EventDataControlFixture : public ::testing::Test
{
  public:
    FakeMemoryResource memory_{};

    static void SetUpTestSuite()
    {
        // we are annotating this requirement coverage for SSR-6225206 on test-suite level, as all
        // test cases within this suite verify the slot-allocation/usage algo defined in SSR-6225206
        RecordProperty("Verifies", "SSR-6225206");
        RecordProperty("Description", "Ensures correct slot allocation algorithm");
        RecordProperty("TestType", "Requirements-based test");
        RecordProperty("Priority", "1");
        RecordProperty("DerivationTechnique", "Analysis of requirements");
    }
};

TEST_F(EventDataControlFixture, CanAllocateOneSlotWithoutContention)
{
    RecordProperty("Verifies", "SCR-5899076");
    RecordProperty("Description", "Ensures that a slot can be allocated");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an initialized EventDataControl structure
    EventDataControl unit{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};

    // When allocating a slot
    auto slot = unit.AllocateNextSlot();

    EXPECT_TRUE(slot.IsValid());
    // The expected (first) slot is returned
    EXPECT_EQ(slot.GetIndex(), 0);
}

TEST_F(EventDataControlFixture, CanAllocateMultipleSlotWithoutContention)
{
    // Given an initialized EventDataControl structure where already a slot is allocated
    EventDataControl unit{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    unit.AllocateNextSlot();

    // When allocating a slot
    const auto slot = unit.AllocateNextSlot();

    // Then the second possible slot is returned
    EXPECT_EQ(slot.GetIndex(), 1);
}

TEST_F(EventDataControlFixture, DiscardedElementOnWritingWillBeInvalid)
{
    // Given an initialized EventDataControl structure where already a slot is allocated
    EventDataControl unit{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    auto slot = unit.AllocateNextSlot();

    // When discarding that slot
    unit.Discard(slot);

    // Then the slot is marked as invalid
    // either accessed via EventDataControl array-elem-access-op
    EXPECT_TRUE(unit[slot.GetIndex()].IsInvalid());
    // or via raw-pointer
    EXPECT_TRUE(EventSlotStatus(slot.GetSlot().load()).IsInvalid());
}

TEST_F(EventDataControlFixture, DiscardedElementAfterWritingIsNotTouched)
{
    // Given an initialized EventDataControl structure where already a slot is written
    EventDataControl unit{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    const auto slot = unit.AllocateNextSlot();
    unit.EventReady(slot, 0x42);

    // When discarding that slot
    unit.Discard(slot);

    // Then the slot is not touched
    EXPECT_EQ(unit[slot.GetIndex()].GetTimeStamp(), 0x42);
    EXPECT_EQ(unit[slot.GetIndex()].GetReferenceCount(), 0);
}

TEST_F(EventDataControlFixture, CanNotAllocateSlotIfAllSlotsAllocated)
{
    // Given an initialized EventDataControl structure where all slots are allocated
    EventDataControl unit{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    for (auto counter = 0; counter < 5; ++counter)
    {
        unit.AllocateNextSlot();
    }

    // When trying to allocate another slot
    const auto slot = unit.AllocateNextSlot();

    // Then this is not possible
    EXPECT_FALSE(slot.IsValid());
}

TEST_F(EventDataControlFixture, CanAllocateSlotAfterOneSlotReady)
{
    // Given an initialized EventDataControl structure where all slots are allocated
    EventDataControl unit{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    std::array<ControlSlotIndicator, 5U> slot_indicators{};
    for (auto counter = 0U; counter < 5U; ++counter)
    {
        slot_indicators[counter] = unit.AllocateNextSlot();
    }

    // When trying freeing one slot and trying to allocate another one
    unit.EventReady(slot_indicators[3], 1);
    const auto slot = unit.AllocateNextSlot();

    // Then the freed slot is allocated
    EXPECT_EQ(slot.GetIndex(), 3);
}

TEST_F(EventDataControlFixture, CanAllocateOldestSlotAfterOneSlotReady)
{
    // Given an initialized EventDataControl structure where all slots are allocated
    EventDataControl unit{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    std::array<ControlSlotIndicator, 5U> slot_indicators{};
    for (auto counter = 0U; counter < 5U; ++counter)
    {
        slot_indicators[counter] = unit.AllocateNextSlot();
    }

    // When trying freeing multiple slots and trying to allocate another one
    unit.EventReady(slot_indicators[4], 3);
    unit.EventReady(slot_indicators[2], 2);
    const auto slot = unit.AllocateNextSlot();

    // Then the oldest (lowest timestamp) slot is allocated
    EXPECT_EQ(slot.GetIndex(), 2);
}

TEST_F(EventDataControlFixture, RandomizedSlotAllocation)
{
    // Given an empty EventDataControl
    EventDataControl unit{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};

    std::atomic<EventSlotStatus::EventTimeStamp> time_stamp{1};
    auto fuzzer = [&unit, &time_stamp]() {
        // Worker that randomly allocates and deallocates random slots, ensuring an increasing time stamp
        std::vector<ControlSlotIndicator> allocated_events{};
        for (int i = 0; i < 1000; i++)
        {
            if (RandomTrueOrFalse())
            {
                const auto slot = unit.AllocateNextSlot();
                if (slot.IsValid())
                {
                    allocated_events.push_back(slot);
                }
            }
            else
            {
                if (!allocated_events.empty())
                {
                    const auto index = RandomNumberBetween(0, allocated_events.size() - 1);
                    unit.EventReady(allocated_events[index], ++time_stamp);
                }
            }
        }
    };

    // When accessing it randomly from multiple threads
    std::vector<std::thread> thread_pool{};
    for (int i = 0; i < 10; i++)
    {
        thread_pool.emplace_back(std::thread{fuzzer});
    }

    for (auto& thread : thread_pool)
    {
        thread.join();
    }

    // Then no race-condition or memory corruption occurs
}

TEST_F(EventDataControlFixture, RegisterProxyElementReturnsValidTransactionLogIndex)
{
    // Given a EventDataControlUnit
    EventDataControl unit{1, memory_.getMemoryResourceProxy(), kMaxSubscribers};

    // When registering the proxy with the TransactionLogSet
    const auto transaction_log_index_result = unit.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId);

    // Then we get a valid transaction log index
    ASSERT_TRUE(transaction_log_index_result.has_value());
    EXPECT_EQ(transaction_log_index_result.value(), 0);
}

TEST_F(EventDataControlFixture, FindNextSlotBlocksAllocation)
{
    // Given a EventDataControlUnit with one ready slot
    EventDataControl unit{1, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    auto slot = unit.AllocateNextSlot();
    unit.EventReady(slot, 1);

    const auto transaction_log_index = unit.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When finding the next slot
    auto event = unit.ReferenceNextEvent(0, transaction_log_index);
    ASSERT_TRUE(event);

    // Then we cannot allocate the slot again
    ASSERT_FALSE(unit.AllocateNextSlot().IsValid());
}

// Re-enable when the test is fixed in Ticket-128552
TEST_F(EventDataControlFixture, DISABLED_MultipleReceiverRefCountCheck)
{
    const std::size_t max_subscribers{10U};

    // Given an EventDataControl with one ready slot
    EventDataControl unit{1, memory_.getMemoryResourceProxy(), max_subscribers};
    auto slot = unit.AllocateNextSlot();
    unit.EventReady(slot, 1);

    auto receiver_tester = [&unit]() {
        const auto transaction_log_index =
            unit.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();
        for (auto counter = 0U; counter < 1000U; counter++)
        {
            // We can increase the ref-count
            auto receive_slot = unit.ReferenceNextEvent(0, transaction_log_index, EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX);
            ASSERT_TRUE(receive_slot.has_value());
            EXPECT_EQ(unit[*receive_slot].GetTimeStamp(), 1);

            // and decrease the ref-count
            unit.DereferenceEvent(receive_slot.value(), transaction_log_index);
        }
    };

    // When accessing it from multiple readers as the same time
    std::vector<std::thread> receiver_threads{};
    for (std::size_t counter = 0; counter < max_subscribers; ++counter)
    {
        receiver_threads.emplace_back(receiver_tester);
    }
    for (auto& thread : receiver_threads)
    {
        thread.join();
    }

    // Then the reference count is zero and we can overwrite the slot if no memory corruption or race occurred
    ASSERT_TRUE(unit.AllocateNextSlot().IsValid());
}

TEST_F(EventDataControlFixture, FailingToUpdateSlotValueCausesReferenceNextEventToReturnNull)
{
    using namespace score::memory::shared;

    AtomicMock<EventSlotStatus::value_type> atomic_mock;
    AtomicIndirectorMock<EventSlotStatus::value_type>::SetMockObject(&atomic_mock);

    constexpr auto max_reference_retries{100U};

    // Given the operation to update the slot value fails max_reference_retries times
    EXPECT_CALL(atomic_mock, compare_exchange_weak(_, _, _)).Times(max_reference_retries).WillRepeatedly(Return(false));

    // and a EventDataControlUnit with one ready slot
    detail_event_data_control::EventDataControlImpl<AtomicIndirectorMock> unit_mock{
        1, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    auto slot = unit_mock.AllocateNextSlot();
    unit_mock.EventReady(slot, 1);

    const auto transaction_log_index =
        unit_mock.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When finding the next slot
    auto event = unit_mock.ReferenceNextEvent(0, transaction_log_index);

    // No event will be found
    ASSERT_FALSE(event.has_value());
}

TEST_F(EventDataControlFixture, GetNumNewEvents_Zero)
{
    // Given an EventDataControl with one ready slot
    EventDataControl unit{1, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    auto slot = unit.AllocateNextSlot();
    unit.EventReady(slot, 1);

    // When checking for new samples since timestamp 1 expect, that 0 is returned.
    EXPECT_EQ(unit.GetNumNewEvents(1), 0);
}

TEST_F(EventDataControlFixture, GetNumNewEvents_One)
{
    // Given an EventDataControl with one ready slot
    EventDataControl unit{1, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    auto slot = unit.AllocateNextSlot();
    unit.EventReady(slot, 1);

    // When checking for new samples since start (timestamp 0) expect, that 1 is returned.
    EXPECT_EQ(unit.GetNumNewEvents(0), 1);
}

TEST_F(EventDataControlFixture, GetNumNewEvents_Many)
{
    // Given an EventDataControl with 6 ready slots
    EventDataControl unit{6, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    for (unsigned int i = 1; i <= 6; i++)
    {
        auto slot = unit.AllocateNextSlot();
        ASSERT_TRUE(slot.IsValid());
        unit.EventReady(slot, i);
    }

    // When checking for new samples since timestamp 0 to 6 expect, that 6 up to 0 is returned.
    EXPECT_EQ(unit.GetNumNewEvents(0), 6);
    EXPECT_EQ(unit.GetNumNewEvents(1), 5);
    EXPECT_EQ(unit.GetNumNewEvents(2), 4);
    EXPECT_EQ(unit.GetNumNewEvents(3), 3);
    EXPECT_EQ(unit.GetNumNewEvents(4), 2);
    EXPECT_EQ(unit.GetNumNewEvents(5), 1);
    EXPECT_EQ(unit.GetNumNewEvents(6), 0);
}

using EventDataControlReferenceSpecificEventFixture = EventDataControlFixture;

TEST_F(EventDataControlReferenceSpecificEventFixture, ReferenceSpecificEvents)
{
    const std::size_t max_number_slots{6U};
    const std::size_t subscription_slots{6U};

    // Given an EventDataControl with 6 ready slots
    EventDataControl unit{max_number_slots, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    for (unsigned int i = 0; i < 6; i++)
    {
        auto slot = unit.AllocateNextSlot();
        ASSERT_TRUE(slot.IsValid());
        unit.EventReady(slot, i + 1);
    }

    const auto transaction_log_index = unit.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When explicitly referencing (ref-count-incrementing) each of them by index this is successful
    for (SlotIndexType i = 0; i < subscription_slots; i++)
    {
        EXPECT_EQ(unit[i].GetReferenceCount(), 0U);
        unit.ReferenceSpecificEvent(i, transaction_log_index);
        EXPECT_EQ(unit[i].GetReferenceCount(), 1U);
    }
}

using EventDataControlReferenceSpecificEventDeathTest = EventDataControlReferenceSpecificEventFixture;
TEST_F(EventDataControlReferenceSpecificEventDeathTest, ReferenceSpecificEvent_StatusInvalidTerminates)
{
    // Given an EventDataControl with one (initially invalid) slot
    EventDataControl unit{1, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    EXPECT_TRUE(unit[0].IsInvalid());

    const auto transaction_log_index = unit.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When explicitly referencing (ref-count-incrementing) it
    // Then the program terminates
    EXPECT_DEATH(unit.ReferenceSpecificEvent(static_cast<SlotIndexType>(0), transaction_log_index), ".*");
}

TEST_F(EventDataControlReferenceSpecificEventDeathTest, ReferenceSpecificEvent_StatusInWritingTerminates)
{
    // Given an EventDataControl with one in_writing slot
    EventDataControl unit{1, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    auto slot = unit.AllocateNextSlot();
    ASSERT_TRUE(slot.IsValid());
    EXPECT_TRUE(unit[slot.GetIndex()].IsInWriting());

    const auto transaction_log_index = unit.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When explicitly referencing (ref-count-incrementing) it
    // Then the program terminates
    EXPECT_DEATH(unit.ReferenceSpecificEvent(static_cast<SlotIndexType>(0), transaction_log_index), ".*");
}

TEST_F(EventDataControlReferenceSpecificEventDeathTest, ReferenceSpecificEvent_ReferenceCountOverFlowsTerminates)
{
    memory::shared::AtomicMock<EventSlotStatus::value_type> atomic_mock;
    memory::shared::AtomicIndirectorMock<EventSlotStatus::value_type>::SetMockObject(&atomic_mock);

    // Expecting that incrementing the current reference count overflows (i.e. the previous value returned by fetch_add
    // was already the max possible value)
    EventSlotStatus event_slot_status_in_writing{};
    event_slot_status_in_writing.SetReferenceCount(kSlotIsInWriting);
    ON_CALL(atomic_mock, fetch_add(_, _))
        .WillByDefault(Return(static_cast<EventSlotStatus::value_type>(event_slot_status_in_writing)));

    // Given an EventDataControl with one slot
    detail_event_data_control::EventDataControlImpl<memory::shared::AtomicIndirectorMock> unit{
        1, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    auto slot = unit.AllocateNextSlot();
    ASSERT_TRUE(slot.IsValid());

    const auto transaction_log_index = unit.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When explicitly referencing (ref-count-incrementing) it which would lead to the ref count overflowing
    // Then the program terminates
    EXPECT_DEATH(unit.ReferenceSpecificEvent(static_cast<SlotIndexType>(0), transaction_log_index), ".*");
}

TEST_F(EventDataControlFixture, AllocatedSlotsCanBeCleanedUp)
{
    RecordProperty("Description", "Tests that all allocated slots can be cleaned up at once.");

    // Given an initialized EventDataControl structure, with allocated slots
    EventDataControl unit{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    const auto first_slot = unit.AllocateNextSlot();
    const auto second_slot = unit.AllocateNextSlot();

    // When cleaning up allocations
    unit.RemoveAllocationsForWriting();

    // Then the allocated slots are no longer in writing
    EXPECT_FALSE(unit[first_slot.GetIndex()].IsInWriting());
    EXPECT_FALSE(unit[second_slot.GetIndex()].IsInWriting());
}

using EventDataControlDeathTest = EventDataControlFixture;
TEST_F(EventDataControlDeathTest, FailingToCleanUpSlotDueToOtherThreadModifyingAtomicTerminates)
{
    memory::shared::AtomicMock<EventSlotStatus::value_type> atomic_mock;
    memory::shared::AtomicIndirectorMock<EventSlotStatus::value_type>::SetMockObject(&atomic_mock);

    // Given that load returns that the slot is in writing
    EventSlotStatus event_slot_status_in_writing{};
    event_slot_status_in_writing.SetReferenceCount(kSlotIsInWriting);
    ON_CALL(atomic_mock, load(_))
        .WillByDefault(Return(static_cast<EventSlotStatus::value_type>(event_slot_status_in_writing)));

    // and that compare_exchange_weak returns false due to another thread modifying the atomic concurrently
    ON_CALL(atomic_mock, compare_exchange_weak(_, _, _)).WillByDefault(Return(false));

    // and given an initialized EventDataControl structure, with a single allocated slot
    detail_event_data_control::EventDataControlImpl<memory::shared::AtomicIndirectorMock> unit{
        kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};

    // When cleaning up allocations
    // Then the program terminates
    EXPECT_DEATH(unit.RemoveAllocationsForWriting(), ".*");
}

struct MultiSenderMultiReceiverParams
{
    SlotIndexType num_slots;
    LolaEventInstanceDeployment::SubscriberCountType num_receiver_threads;
    std::size_t num_actions_per_sender;
    std::size_t num_actions_per_receiver;
    std::size_t max_referenced_samples_per_receiver;
};

class MultiSenderMultiReceiverTest : public ::testing::TestWithParam<MultiSenderMultiReceiverParams>
{
  public:
    MultiSenderMultiReceiverTest()
    {
        EventDataControl::ResetPerformanceCounters();
    }
    ~MultiSenderMultiReceiverTest() override
    {
        EventDataControl::DumpPerformanceCounters();
    }

  protected:
    // Protected by lock_
    std::mutex lock_{};
    EventSlotStatus::EventTimeStamp next_ts_{1};
    std::set<SlotIndexType> allocated_slots_{};

    // Unprotected
    FakeMemoryResource memory_{};
    EventDataControl unit_{GetParam().num_slots, memory_.getMemoryResourceProxy(), GetParam().num_receiver_threads};
};

TEST_P(MultiSenderMultiReceiverTest, MultiSenderMultiReceiver)
{
    // this test case access the data structure in parallel by multiple threads one from read and one thread from writer
    // side, this way we can ensure that _no_ data races occur

    auto sender = [this]() {
        // Randomly allocate or mark slot as ready
        const auto& params = GetParam();
        EventSlotStatus::EventTimeStamp ts{1};
        for (std::size_t counter = 0; counter < params.num_actions_per_sender; ++counter)
        {
            auto slot = unit_.AllocateNextSlot();
            ASSERT_TRUE(slot.IsValid());

            unit_.EventReady(slot, ++ts);
        }
    };

    auto receiver = [this]() {
        // randomly increases or decreases ref-count
        const auto& params = GetParam();

        std::set<SlotIndexType> used_slots{};
        EventSlotStatus::EventTimeStamp start_ts{1};

        TransactionLogSet::TransactionLogIndex transaction_log_index =
            unit_.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

        for (std::size_t counter = 0; counter < params.num_actions_per_receiver; ++counter)
        {
            if (used_slots.size() < params.max_referenced_samples_per_receiver && RandomTrueOrFalse())
            {
                auto slot = unit_.ReferenceNextEvent(start_ts, transaction_log_index);
                if (slot.has_value())
                {
                    start_ts = EventSlotStatus{unit_[slot.value()]}.GetTimeStamp();
                    used_slots.insert(slot.value());
                }
            }
            else
            {
                if (!used_slots.empty())
                {
                    auto index = RandomNumberBetween(0, used_slots.size() - 1);
                    auto iter = used_slots.begin();
                    std::advance(iter, index);
                    auto event_slot = *iter;
                    used_slots.erase(iter);
                    unit_.DereferenceEvent(event_slot, transaction_log_index);
                }
            }
        }
    };

    auto sender_thread = std::thread{sender};

    const auto& params = GetParam();
    std::vector<std::thread> thread_pool{};
    for (auto counter = 0U; counter < params.num_receiver_threads; counter++)
    {
        thread_pool.emplace_back(receiver);
    }

    sender_thread.join();
    for (auto& thread : thread_pool)
    {
        thread.join();
    }
}

// Re-enable when the test is fixed in Ticket-128552
TEST_P(MultiSenderMultiReceiverTest, DISABLED_MultiSenderMultiReceiverMaxReceiverContention)
{
    // this test case increases the contention between reader and writer, by ensuring that the reader will hold his max.
    // possible sample before freeing one.
    std::atomic<bool> stop_sender{false};

    auto sender = [this, &stop_sender]() {
        // Randomly allocate or mark slot as ready
        EventSlotStatus::EventTimeStamp ts{1};
        while (!stop_sender)
        {
            auto slot = unit_.AllocateNextSlot();
            ASSERT_NE(ts, std::numeric_limits<std::uint32_t>::max());
            if (!slot.IsValid())
            {
                EventDataControl::DumpPerformanceCounters();
                /* Terminate call tolerated.See Assumptions of Use in mw/com/design/README.md*/
                std::terminate();
            }
            else
            {
                unit_.EventReady(slot, ++ts);
            }
            EXPECT_TRUE(slot.IsValid());
        }
    };

    auto receiver = [this]() {
        // randomly increases or decreases ref-count
        const auto& params = GetParam();

        std::set<SlotIndexType> used_slots{};
        EventSlotStatus::EventTimeStamp start_ts{0};

        TransactionLogSet::TransactionLogIndex transaction_log_index =
            unit_.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

        for (std::size_t counter = 0; counter < params.num_actions_per_receiver; ++counter)
        {
            EventSlotStatus::EventTimeStamp highest_ts{EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX};
            std::optional<SlotIndexType> slot_last;
            std::size_t counter1 = 0;
            while (counter1 <= params.max_referenced_samples_per_receiver &&
                   used_slots.size() < params.max_referenced_samples_per_receiver)
            {
                auto slot = unit_.ReferenceNextEvent(start_ts, transaction_log_index, highest_ts);
                if (slot.has_value())
                {
                    highest_ts = EventSlotStatus{unit_[slot.value()]}.GetTimeStamp();
                    used_slots.insert(slot.value());
                    slot_last = slot;
                }
                counter1++;
            }

            if (slot_last.has_value())
            {
                start_ts = EventSlotStatus{unit_[slot_last.value()]}.GetTimeStamp();
            }

            if (used_slots.size() == params.max_referenced_samples_per_receiver)
            {

                auto index = RandomNumberBetween(0, used_slots.size() - 1);
                auto iter = used_slots.begin();
                std::advance(iter, index);
                auto event_slot = *iter;
                used_slots.erase(iter);
                unit_.DereferenceEvent(event_slot, transaction_log_index);
            }
        }
    };

    auto sender_thread = std::thread{sender};

    const auto& params = GetParam();
    std::vector<std::thread> thread_pool{};
    for (auto counter = 0U; counter < params.num_receiver_threads; counter++)
    {
        thread_pool.emplace_back(receiver);
    }

    for (auto& thread : thread_pool)
    {
        thread.join();
    }
    stop_sender = true;
    sender_thread.join();
}

INSTANTIATE_TEST_SUITE_P(MultiSenderMultiReceiverRealisticCases,
                         MultiSenderMultiReceiverTest,
                         ::testing::Values(
                             // Values:
                             // num_slots, num_receiver_threads, num_actions_per_sender, num_actions_per_receiver,
                             // max_referenced_samples_per_receiver
                             MultiSenderMultiReceiverParams{21, 5, 100, 5, 4}
                             //                            MultiSenderMultiReceiverParams{2001, 500, 100, 100, 4},
                             //                            MultiSenderMultiReceiverParams{121, 30, 100, 1000, 4}
                             ));

}  // namespace
}  // namespace score::mw::com::impl::lola
