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
#include "score/mw/com/impl/bindings/lola/skeleton_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include "score/memory/shared/atomic_indirector.h"
#include "score/memory/shared/atomic_mock.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <random>
#include <thread>
#include <vector>

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

class SkeletonEventDataControlLocalViewFixture : public ::testing::Test
{
  public:
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

    void TearDown() override
    {
        memory::shared::AtomicIndirectorMock<EventSlotStatus::value_type>::SetMockObject(nullptr);
    }

    SkeletonEventDataControlLocalViewFixture& GivenARealSkeletonEventDataControlLocalView(
        const SlotIndexType max_slots,
        const LolaEventInstanceDeployment::SubscriberCountType max_subscribers)
    {
        event_data_control_ = std::make_unique<EventDataControl>(max_slots, memory_, max_subscribers);
        unit_ = std::make_unique<SkeletonEventDataControlLocalView<>>(*event_data_control_);

        return *this;
    }

    SkeletonEventDataControlLocalViewFixture& GivenAMockedEventDataControl(
        const SlotIndexType max_slots,
        const LolaEventInstanceDeployment::SubscriberCountType max_subscribers)
    {
        event_data_control_ = std::make_unique<EventDataControl>(max_slots, memory_, max_subscribers);

        atomic_mock_ = std::make_unique<memory::shared::AtomicMock<EventSlotStatus::value_type>>();
        memory::shared::AtomicIndirectorMock<EventSlotStatus::value_type>::SetMockObject(atomic_mock_.get());

        unit_mock_ = std::make_unique<SkeletonEventDataControlLocalView<memory::shared::AtomicIndirectorMock>>(
            *event_data_control_);

        return *this;
    }

    SlotIndexType WithAnAllocatedSlot(EventSlotStatus::EventTimeStamp timestamp = 1)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(unit_ != nullptr);
        auto slot = unit_->AllocateNextSlot();
        EXPECT_TRUE(slot.IsValid());
        unit_->EventReady(slot, timestamp);
        return slot.GetIndex();
    }

    FakeMemoryResource memory_{};
    std::unique_ptr<memory::shared::AtomicMock<EventSlotStatus::value_type>> atomic_mock_{nullptr};

    std::unique_ptr<EventDataControl> event_data_control_{nullptr};
    std::unique_ptr<SkeletonEventDataControlLocalView<>> unit_{nullptr};
    std::unique_ptr<SkeletonEventDataControlLocalView<memory::shared::AtomicIndirectorMock>> unit_mock_{nullptr};
};

TEST_F(SkeletonEventDataControlLocalViewFixture, CanAllocateOneSlotWithoutContention)
{
    RecordProperty("Verifies", "SCR-5899076");
    RecordProperty("Description", "Ensures that a slot can be allocated");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an initialized EventDataControl structure
    GivenARealSkeletonEventDataControlLocalView(kMaxSlots, kMaxSubscribers);

    // When allocating a slot
    auto slot = unit_->AllocateNextSlot();

    EXPECT_TRUE(slot.IsValid());
    // The expected (first) slot is returned
    EXPECT_EQ(slot.GetIndex(), 0);
}

TEST_F(SkeletonEventDataControlLocalViewFixture, CanAllocateOneSlotWhenReferenceCountChanges)
{
    RecordProperty("Verifies", "SCR-5899076");
    RecordProperty("Description", "Ensures that a slot can be allocated");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an initialized EventDataControl structure
    score::cpp::ignore = GivenAMockedEventDataControl(kMaxSlots, kMaxSubscribers);

    // And an atomic mimicking two relevant slots where ...
    EXPECT_CALL(*atomic_mock_, load(_))
        // ... first slot is unreferenced for the first iteration in FindOldestUnusedSlot()
        .WillOnce([] {
            EventSlotStatus invalid_event_slot_status{};
            return static_cast<EventSlotStatus::value_type>(invalid_event_slot_status);
        })
        // ... first slot was set to in-writing by a different party at the last possible time right before we check in
        // AllocateNextSlot()
        .WillOnce([] {
            EventSlotStatus event_slot_status_in_writing{};
            event_slot_status_in_writing.SetReferenceCount(kSlotIsInWriting);
            return static_cast<EventSlotStatus::value_type>(event_slot_status_in_writing);
        })
        // ... first slot keeps in-writing status during next iteration in FindOldestUnusedSlot() and therefore won't be
        // chosen
        .WillOnce([] {
            EventSlotStatus event_slot_status_in_writing{};
            event_slot_status_in_writing.SetReferenceCount(kSlotIsInWriting);
            return static_cast<EventSlotStatus::value_type>(event_slot_status_in_writing);
        })
        // ... second slot is always unreferenced in all future calls and is therefore chosen
        .WillRepeatedly([] {
            EventSlotStatus invalid_event_slot_status{};
            return static_cast<EventSlotStatus::value_type>(invalid_event_slot_status);
        });

    // When allocating a slot
    const auto slot = unit_mock_->AllocateNextSlot();

    EXPECT_TRUE(slot.IsValid());
    // The expected (second) slot is returned
    EXPECT_EQ(slot.GetIndex(), 1);
}

TEST_F(SkeletonEventDataControlLocalViewFixture, CanAllocateMultipleSlotWithoutContention)
{
    // Given an initialized EventDataControl structure where already a slot is allocated
    GivenARealSkeletonEventDataControlLocalView(kMaxSlots, kMaxSubscribers);
    unit_->AllocateNextSlot();

    // When allocating a slot
    const auto slot = unit_->AllocateNextSlot();

    // Then the second possible slot is returned
    EXPECT_EQ(slot.GetIndex(), 1);
}

TEST_F(SkeletonEventDataControlLocalViewFixture, DiscardedElementOnWritingWillBeInvalid)
{
    // Given an initialized EventDataControl structure where already a slot is allocated
    GivenARealSkeletonEventDataControlLocalView(kMaxSlots, kMaxSubscribers);
    auto slot = unit_->AllocateNextSlot();

    // When discarding that slot
    unit_->Discard(slot);

    // Then the slot is marked as invalid
    // either accessed via EventDataControl array-elem-access-op
    EXPECT_TRUE((*unit_)[slot.GetIndex()].IsInvalid());
    // or via raw-pointer
    EXPECT_TRUE(EventSlotStatus(slot.GetSlot().load()).IsInvalid());
}

TEST_F(SkeletonEventDataControlLocalViewFixture, DiscardedElementAfterWritingIsNotTouched)
{
    // Given an initialized EventDataControl structure where already a slot is written
    GivenARealSkeletonEventDataControlLocalView(kMaxSlots, kMaxSubscribers);
    const auto slot = unit_->AllocateNextSlot();
    unit_->EventReady(slot, 0x42);

    // When discarding that slot
    unit_->Discard(slot);

    // Then the slot is not touched
    EXPECT_EQ((*unit_)[slot.GetIndex()].GetTimeStamp(), 0x42);
    EXPECT_EQ((*unit_)[slot.GetIndex()].GetReferenceCount(), 0);
}

TEST_F(SkeletonEventDataControlLocalViewFixture, CanNotAllocateSlotIfAllSlotsAllocated)
{
    // Given an initialized EventDataControl structure where all slots are allocated
    GivenARealSkeletonEventDataControlLocalView(kMaxSlots, kMaxSubscribers);
    for (auto counter = 0; counter < 5; ++counter)
    {
        unit_->AllocateNextSlot();
    }

    // When trying to allocate another slot
    const auto slot = unit_->AllocateNextSlot();

    // Then this is not possible
    EXPECT_FALSE(slot.IsValid());
}

TEST_F(SkeletonEventDataControlLocalViewFixture, CanAllocateSlotAfterOneSlotReady)
{
    // Given an initialized EventDataControl structure where all slots are allocated
    GivenARealSkeletonEventDataControlLocalView(kMaxSlots, kMaxSubscribers);
    std::array<ControlSlotIndicator, 5U> slot_indicators{};
    for (auto counter = 0U; counter < 5U; ++counter)
    {
        slot_indicators[counter] = unit_->AllocateNextSlot();
    }

    // When trying freeing one slot and trying to allocate another one
    unit_->EventReady(slot_indicators[3], 1);
    const auto slot = unit_->AllocateNextSlot();

    // Then the freed slot is allocated
    EXPECT_EQ(slot.GetIndex(), 3);
}

TEST_F(SkeletonEventDataControlLocalViewFixture, CanAllocateOldestSlotAfterOneSlotReady)
{
    // Given an initialized EventDataControl structure where all slots are allocated
    GivenARealSkeletonEventDataControlLocalView(kMaxSlots, kMaxSubscribers);
    std::array<ControlSlotIndicator, 5U> slot_indicators{};
    for (auto counter = 0U; counter < 5U; ++counter)
    {
        slot_indicators[counter] = unit_->AllocateNextSlot();
    }

    // When trying freeing multiple slots and trying to allocate another one
    unit_->EventReady(slot_indicators[4], 3);
    unit_->EventReady(slot_indicators[2], 2);
    const auto slot = unit_->AllocateNextSlot();

    // Then the oldest (lowest timestamp) slot is allocated
    EXPECT_EQ(slot.GetIndex(), 2);
}

// Initially we had a 'randomized allocate or free logic'.
// But because of our excessive retry-logic (Ticket-188373) in case of slot exhaustion,
// this lead to huge test runtimes/timeouts.
// So we now favor an alternating allocate/free approach, which avoids this issue
TEST_F(SkeletonEventDataControlLocalViewFixture, MultithreadedSlotAllocationDeallocation)
{
    // Given an empty EventDataControl
    GivenARealSkeletonEventDataControlLocalView(kMaxSlots, kMaxSubscribers);

    std::atomic<EventSlotStatus::EventTimeStamp> time_stamp{1};
    auto fuzzer = [this, &time_stamp]() {
        // Worker that alternates between slot allocation and deallocation, ensuring an increasing time stamp
        std::vector<ControlSlotIndicator> allocated_events{};
        bool allocate{false};
        for (int i = 0; i < 1000; i++)
        {
            if ((allocate = !allocate))
            {
                const auto slot = unit_->AllocateNextSlot();
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
                    unit_->EventReady(allocated_events[index], ++time_stamp);
                }
            }
        }
    };

    // When accessing it randomly from multiple threads
    std::vector<std::thread> thread_pool{};
    for (int i = 0; i < 10; i++)
    {
        thread_pool.emplace_back(fuzzer);
    }

    for (auto& thread : thread_pool)
    {
        thread.join();
    }

    // Then no race-condition or memory corruption occurs
}

TEST_F(SkeletonEventDataControlLocalViewFixture, AllocatedSlotsCanBeCleanedUp)
{
    RecordProperty("Description", "Tests that all allocated slots can be cleaned up at once.");

    // Given an initialized EventDataControl structure, with allocated slots
    GivenARealSkeletonEventDataControlLocalView(kMaxSlots, kMaxSubscribers);
    const auto first_slot = unit_->AllocateNextSlot();
    const auto second_slot = unit_->AllocateNextSlot();

    // When cleaning up allocations
    unit_->RemoveAllocationsForWriting();

    // Then the allocated slots are no longer in writing
    EXPECT_FALSE((*unit_)[first_slot.GetIndex()].IsInWriting());
    EXPECT_FALSE((*unit_)[second_slot.GetIndex()].IsInWriting());
}

using EventDataControlDeathTest = SkeletonEventDataControlLocalViewFixture;
TEST_F(EventDataControlDeathTest, FailingToCleanUpSlotDueToOtherThreadModifyingAtomicTerminates)
{
    GivenAMockedEventDataControl(kMaxSlots, kMaxSubscribers);

    // and given that load returns that the slot is in writing
    EventSlotStatus event_slot_status_in_writing{};
    event_slot_status_in_writing.SetReferenceCount(kSlotIsInWriting);
    ON_CALL(*atomic_mock_, load(_))
        .WillByDefault(Return(static_cast<EventSlotStatus::value_type>(event_slot_status_in_writing)));

    // and that compare_exchange_weak returns false due to another thread modifying the atomic concurrently
    ON_CALL(*atomic_mock_, compare_exchange_weak(_, _, _)).WillByDefault(Return(false));

    // When cleaning up allocations
    // Then the program terminates
    EXPECT_DEATH(unit_mock_->RemoveAllocationsForWriting(), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
