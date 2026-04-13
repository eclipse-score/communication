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
#include "score/mw/com/impl/bindings/lola/event_data_control_composite.h"

#include "score/mw/com/impl/bindings/lola/control_slot_types.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/proxy_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/transaction_log_set.h"
#include "score/mw/com/impl/instance_specifier.h"

#include "score/memory/shared/atomic_indirector.h"
#include "score/memory/shared/atomic_mock.h"
#include "score/memory/shared/new_delete_delegate_resource.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <set>
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

const TransactionLogId kDummyTransactionLogIdQm{10U};
const TransactionLogId kDummyTransactionLogIdAsil{10U};

std::size_t RandomNumberBetween(const std::size_t lower, const std::size_t upper)
{
    const auto time_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::seed_seq sequence_seed{std::uint32_t(time_seed & 0xFFFFFFFFU), std::uint32_t(time_seed >> 32)};
    std::mt19937_64 random_number_generator{sequence_seed};
    std::uniform_int_distribution<std::size_t> number_distribution(lower, upper);
    return number_distribution(random_number_generator);
}

bool RandomTrueOrFalse()
{
    return RandomNumberBetween(0, 1);
}

const std::uint64_t kMemoryResourceId{10U};

class EventDataControlCompositeFixture : public ::testing::Test
{
  public:
    static constexpr std::uint8_t kSlotCount{5U};
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

    EventDataControlCompositeFixture& WithQmAndAsilBEventDataControls()
    {
        qm_ = std::make_unique<EventDataControl>(kMaxSlots, memory_, kMaxSubscribers);
        asil_ = std::make_unique<EventDataControl>(kMaxSlots, memory_, kMaxSubscribers);

        skeleton_qm_local_.emplace(*qm_);
        skeleton_asil_local_.emplace(*asil_);

        proxy_qm_local_.emplace(*qm_);
        proxy_asil_local_.emplace(*asil_);

        return *this;
    }

    EventDataControlCompositeFixture& WithQmOnlyEventDataControl()
    {
        qm_ = std::make_unique<EventDataControl>(kMaxSlots, memory_, kMaxSubscribers);
        skeleton_qm_local_.emplace(*qm_);
        proxy_qm_local_.emplace(*qm_);
        return *this;
    }

    EventDataControlCompositeFixture& WithEventDataControlCompositeUsingRealAtomics()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(skeleton_qm_local_.has_value());

        auto* const asil_control = skeleton_asil_local_.has_value() ? &skeleton_asil_local_.value() : nullptr;
        unit_ = std::make_unique<EventDataControlComposite<>>(skeleton_qm_local_.value(), asil_control, nullptr);
        return *this;
    }

    EventDataControlCompositeFixture& WithEventDataControlCompositeUsingMockedAtomics()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(skeleton_qm_local_.has_value());

        atomic_mock_ = std::make_unique<memory::shared::AtomicMock<EventSlotStatus::value_type>>();
        memory::shared::AtomicIndirectorMock<EventSlotStatus::value_type>::SetMockObject(atomic_mock_.get());

        auto* const skeleton_asil_local_ptr =
            skeleton_asil_local_.has_value() ? &skeleton_asil_local_.value() : nullptr;
        unit_with_mock_atomics_ = std::make_unique<EventDataControlComposite<memory::shared::AtomicIndirectorMock>>(
            skeleton_qm_local_.value(), skeleton_asil_local_ptr, nullptr);

        return *this;
    }

    EventDataControlCompositeFixture& WithARegisteredTransactionLog()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(qm_ != nullptr);
        transaction_log_index_qm_ = std::make_unique<TransactionLogSet::TransactionLogIndex>(
            skeleton_qm_local_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogIdQm).value());

        if (asil_ != nullptr)
        {
            transaction_log_index_asil_ = std::make_unique<TransactionLogSet::TransactionLogIndex>(
                skeleton_asil_local_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogIdAsil).value());
        }
        return *this;
    }

    void AllocateAllSlots()
    {
        for (auto counter = 0U; counter < kSlotCount; ++counter)
        {
            score::cpp::ignore = unit_->AllocateNextSlot();
        }
    }

    void ReadyAllSlots()
    {
        for (SlotIndexType counter = 0; counter < kSlotCount; ++counter)
        {
            unit_->EventReady(counter, counter + 1u);
        }
    }

    void ReadyAllQmSlotsOnly()
    {
        for (SlotIndexType counter = 0; counter < kSlotCount; ++counter)
        {
            skeleton_qm_local_->EventReady(counter, counter + 1u);
        }
    }

    memory::shared::NewDeleteDelegateMemoryResource memory_{kMemoryResourceId};

    std::unique_ptr<EventDataControl> asil_{nullptr};
    std::unique_ptr<EventDataControl> qm_{nullptr};
    std::optional<SkeletonEventDataControlLocalView<>> skeleton_qm_local_{};
    std::optional<SkeletonEventDataControlLocalView<>> skeleton_asil_local_{};
    std::optional<ProxyEventDataControlLocalView<>> proxy_qm_local_{};
    std::optional<ProxyEventDataControlLocalView<>> proxy_asil_local_{};

    std::unique_ptr<EventDataControlComposite<>> unit_{nullptr};

    std::unique_ptr<memory::shared::AtomicMock<EventSlotStatus::value_type>> atomic_mock_{nullptr};
    std::unique_ptr<EventDataControlComposite<memory::shared::AtomicIndirectorMock>> unit_with_mock_atomics_{nullptr};

    std::unique_ptr<TransactionLogSet::TransactionLogIndex> transaction_log_index_qm_{nullptr};
    std::unique_ptr<TransactionLogSet::TransactionLogIndex> transaction_log_index_asil_{nullptr};
};

TEST_F(EventDataControlCompositeFixture, CanCreateAndDestroyFixture) {}

TEST_F(EventDataControlCompositeFixture, CanAllocateOneSlot)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // When allocating one slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the first slot is used
    EXPECT_EQ(allocation.allocated_slot_index.value(), 0);

    // And there was no indication of QM misbehaviour
    EXPECT_FALSE(allocation.qm_misbehaved);
}

TEST_F(EventDataControlCompositeFixture, GetLatestTimeStampReturnCorrectValue)
{
    // Given an EventDataControlComposite with 1 allocated slots that are set to ready
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();
    score::cpp::ignore = unit_->AllocateNextSlot();
    const EventSlotStatus::EventTimeStamp time_stamp{2};
    unit_->EventReady(SlotIndexType{0U}, time_stamp);

    // When acquiring the latest timestamp
    const auto time_stamp_obtained = unit_->GetLatestTimestamp();

    // Then the timestamp is equal to 2
    EXPECT_EQ(time_stamp_obtained, time_stamp);
}

TEST_F(EventDataControlCompositeFixture, CanAllocateMultipleSlots)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // Then it is possible to allocate 2 slots.
    const auto allocation_slot_1 = unit_->AllocateNextSlot();
    const SlotIndexType slot_1_index{0};
    ASSERT_EQ(allocation_slot_1.allocated_slot_index.value(), slot_1_index);
    const auto allocation_slot_2 = unit_->AllocateNextSlot();
    const SlotIndexType slot_2_index{1};
    ASSERT_EQ(allocation_slot_2.allocated_slot_index.value(), slot_2_index);
}

TEST_F(EventDataControlCompositeFixture, GetLatestTimeStampReturnCorrectValueIfOneSlotIsMarkedAsInvalid)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // When allocating 2 slots
    auto allocation_result = unit_->AllocateNextSlot();
    auto allocation_result_2 = unit_->AllocateNextSlot();

    // and set slot 1 to ready
    EventSlotStatus::EventTimeStamp time_stamp{2};
    unit_->EventReady(allocation_result.allocated_slot_index.value(), time_stamp);

    // and discarding slot 2
    unit_->Discard(allocation_result.allocated_slot_index.value());

    // Then the timestamp is equal to 2
    time_stamp = unit_->GetLatestTimestamp();
    EventSlotStatus::EventTimeStamp expected_time_stamp{2};
    EXPECT_EQ(time_stamp, expected_time_stamp);
}

TEST_F(EventDataControlCompositeFixture, GetLatestTimeStampReturnsDefaultValuesIfAllSlotAreInvalid)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // When allocating 2 slots
    auto allocation_result = unit_->AllocateNextSlot();
    auto allocation_result_2 = unit_->AllocateNextSlot();

    // and discarding them
    unit_->Discard(allocation_result.allocated_slot_index.value());
    unit_->Discard(allocation_result_2.allocated_slot_index.value());

    // Then the timestamp equal to 1
    auto time_stamp = unit_->GetLatestTimestamp();
    EventSlotStatus::EventTimeStamp expected_time_stamp{1};
    EXPECT_EQ(time_stamp, expected_time_stamp);
}

TEST_F(EventDataControlCompositeFixture, GetLatestTimeStampReturnsDefaultValuesIfNoSlotHaveBeenAllocated)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // Then the timestamp equal to 1
    auto time_stamp = unit_->GetLatestTimestamp();
    EventSlotStatus::EventTimeStamp expected_time_stamp{1};
    EXPECT_EQ(time_stamp, expected_time_stamp);
}

TEST_F(EventDataControlCompositeFixture,
       GetLatestTimeStampReturnsDefaultValuesIfASlotHasBeenAllocatedButNotMarkedAsReady)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // When allocating 1 slot
    score::cpp::ignore = unit_->AllocateNextSlot();

    // Then the timestamp equal to 1
    auto time_stamp = unit_->GetLatestTimestamp();
    EventSlotStatus::EventTimeStamp expected_time_stamp{1};
    EXPECT_EQ(time_stamp, expected_time_stamp);
}

TEST_F(EventDataControlCompositeFixture, FailingToLockQmMultiSlotAllocatesOnlyAsilBSlot)
{
    constexpr std::size_t max_multi_allocate_count{100U};

    // Given an EventDataControlComposite which mocks atomic operations with zero used slots
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingMockedAtomics();

    // and given that the operation to update the QM slot value fails max_multi_allocate_count times
    EXPECT_CALL(*atomic_mock_, compare_exchange_strong(_, _, _))
        .Times(max_multi_allocate_count)
        .WillRepeatedly(Return(false));

    // When allocating one slot
    const auto allocation = unit_with_mock_atomics_->AllocateNextSlot();

    // Then it tries to allocate the asil b slot again and the first slot is still used
    EXPECT_EQ(allocation.allocated_slot_index.value(), 0);

    // And there is an indication of QM misbehaviour. I.e. allocation only contains a valid ASIL-B slot pointer, but no
    // QM slot pointer.
    EXPECT_TRUE(allocation.qm_misbehaved);
}

TEST_F(EventDataControlCompositeFixture, FailingToLockAsilMultiSlotStillAllocatesAsilBSlot)
{
    ::testing::InSequence sequence{};
    constexpr std::size_t max_multi_allocate_count{100U};

    // Given an EventDataControlComposite which mocks atomic operations with zero used slots
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingMockedAtomics();

    // Given the operation to update the QM slot value succeeds but the operation to update the asil B slot fails
    // max_multi_allocate_count times
    for (std::size_t i = 0; i < max_multi_allocate_count; ++i)
    {
        EXPECT_CALL(*atomic_mock_, compare_exchange_strong(_, _, _)).WillOnce(Return(true));
        EXPECT_CALL(*atomic_mock_, compare_exchange_strong(_, _, _)).WillOnce(Return(false));
    }

    // When allocating one slot
    const auto allocation = unit_with_mock_atomics_->AllocateNextSlot();

    // Then it tries to allocate the asil b slot again and the first slot is still used
    EXPECT_EQ(allocation.allocated_slot_index.value(), 0);

    // And there is an indication of QM misbehaviour
    EXPECT_TRUE(allocation.qm_misbehaved);
}

TEST_F(EventDataControlCompositeFixture, CanAllocateOneSlotOnlyForQm)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmOnlyEventDataControl().WithEventDataControlCompositeUsingRealAtomics();

    // When allocating one slot
    auto allocation = unit_->AllocateNextSlot();

    // Then the first slot is used
    EXPECT_EQ(allocation.allocated_slot_index.value(), 0);
}

TEST_F(EventDataControlCompositeFixture, CanAllocateOneSlotWhenAlreadyOneIsAllocated)
{
    // Given an EventDataControlComposite with only one used slot
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();
    score::cpp::ignore = unit_->AllocateNextSlot();

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the second slot is used
    EXPECT_EQ(allocation.allocated_slot_index.value(), 1);
}

TEST_F(EventDataControlCompositeFixture, OverWritesOldestSample)
{
    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();
    AllocateAllSlots();
    const SlotIndexType ready_slot_index{3U};
    unit_->EventReady(ready_slot_index, EventSlotStatus::EventTimeStamp{1});

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was marked ready
    EXPECT_EQ(allocation.allocated_slot_index.value(), ready_slot_index);
}

TEST_F(EventDataControlCompositeFixture, OverWritesDiscardedEvent)
{
    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();
    AllocateAllSlots();
    const SlotIndexType discarded_slot_index{3U};
    unit_->Discard(discarded_slot_index);

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was marked ready
    EXPECT_EQ(allocation.allocated_slot_index.value(), discarded_slot_index);
}

TEST_F(EventDataControlCompositeFixture, OverWritesOldestSampleOnlyForQm)
{

    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    WithQmOnlyEventDataControl().WithEventDataControlCompositeUsingRealAtomics();
    AllocateAllSlots();
    const SlotIndexType ready_slot_index{3U};
    unit_->EventReady(ready_slot_index, EventSlotStatus::EventTimeStamp{1});

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was marked ready
    EXPECT_EQ(allocation.allocated_slot_index.value(), ready_slot_index);
}

TEST_F(EventDataControlCompositeFixture, SkipsEventIfUsedInQmList)
{
    // Given an EventDataControlComposite with all slots written at one time, and only one unused and a registered
    // transaction log
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics().WithARegisteredTransactionLog();
    AllocateAllSlots();
    const SlotIndexType first_ready_slot_index{2U};
    const SlotIndexType second_ready_slot_index{4U};
    unit_->EventReady(first_ready_slot_index, EventSlotStatus::EventTimeStamp{1});
    unit_->EventReady(second_ready_slot_index, EventSlotStatus::EventTimeStamp{2});

    const EventSlotStatus::EventTimeStamp last_search_time{0};
    score::cpp::ignore =
        proxy_qm_local_->ReferenceNextEvent(last_search_time, *transaction_log_index_qm_);  // slot 4 is used in QM list

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was unused
    EXPECT_EQ(allocation.allocated_slot_index.value(), first_ready_slot_index);
}

TEST_F(EventDataControlCompositeFixture, SkipsEventIfUsedInAsilList)
{
    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics().WithARegisteredTransactionLog();
    AllocateAllSlots();
    const SlotIndexType first_ready_slot_index{2U};
    const SlotIndexType second_ready_slot_index{4U};
    unit_->EventReady(first_ready_slot_index, EventSlotStatus::EventTimeStamp{1});
    unit_->EventReady(second_ready_slot_index, EventSlotStatus::EventTimeStamp{2});

    const EventSlotStatus::EventTimeStamp last_search_time{0};
    score::cpp::ignore = proxy_asil_local_->ReferenceNextEvent(
        last_search_time, *transaction_log_index_asil_);  // slot 4 is used in ASIL list

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was only unused
    EXPECT_EQ(allocation.allocated_slot_index.value(), first_ready_slot_index);
}

TEST_F(EventDataControlCompositeFixture, ReturnsNoSlotIfAllUsed)
{
    // Given an EventDataControlComposite in which all slots are used
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();
    AllocateAllSlots();

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then no slot is found
    EXPECT_FALSE(allocation.allocated_slot_index.has_value());
}

TEST_F(EventDataControlCompositeFixture, ReturnsNoSlotIfAllUsedQMOnly)
{
    // Given an EventDataControlComposite containing only a QM EventDataControl in which all slots are used
    WithQmOnlyEventDataControl().WithEventDataControlCompositeUsingRealAtomics();
    AllocateAllSlots();

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then no slot is found
    EXPECT_FALSE(allocation.allocated_slot_index.has_value());
}

TEST_F(EventDataControlCompositeFixture, QmConsumerViolation)
{
    RecordProperty("Verifies", "SCR-5899299, SCR-5899292");
    RecordProperty("Description", "Checks whether a QM process is ignored if he is miss behaving");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an EventDataControlComposite with all slots ready
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics().WithARegisteredTransactionLog();
    AllocateAllSlots();
    ReadyAllSlots();
    ASSERT_FALSE(unit_->IsQmControlDisconnected());
    // and a QM consumer, which blocks/references ALL slots
    auto upper_limit = EventSlotStatus::TIMESTAMP_MAX;
    for (auto counter = 0; counter < 5; ++counter)
    {
        auto slot_index = proxy_qm_local_->ReferenceNextEvent(0, *transaction_log_index_qm_, upper_limit);
        upper_limit = (*proxy_qm_local_)[slot_index.value()].GetTimeStamp();
    }

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then still a slot is found
    EXPECT_TRUE(allocation.allocated_slot_index.has_value());
}

TEST_F(EventDataControlCompositeFixture, AllocationIgnoresQMAfterContractViolation)
{
    RecordProperty("Verifies", "SCR-5899299, SCR-5899292");
    RecordProperty("Description", "Checks whether a QM process is ignored if he is miss behaving");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an EventDataControlComposite with all slots ready
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics().WithARegisteredTransactionLog();
    AllocateAllSlots();
    ReadyAllSlots();

    // and a QM consumer, which blocks/references ALL slots and thus already violated the contract
    auto upper_limit = EventSlotStatus::TIMESTAMP_MAX;
    for (auto counter = 0; counter < 5; ++counter)
    {
        auto slot_index = proxy_qm_local_->ReferenceNextEvent(0, *transaction_log_index_qm_, upper_limit);
        upper_limit = (*proxy_qm_local_)[slot_index.value()].GetTimeStamp();
    }
    score::cpp::ignore = unit_->AllocateNextSlot();
    ASSERT_TRUE(unit_->IsQmControlDisconnected());

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then still a slot could be allocated for ASIL-B
    EXPECT_TRUE(allocation.allocated_slot_index.has_value());
}

TEST_F(EventDataControlCompositeFixture, ReturnsNoSlotIfAllUsedAfterQmDisconnect)
{
    // Given an EventDataControlComposite containing only a QM EventDataControl in which all slots are used
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();
    AllocateAllSlots();

    // and given that QmControlDisconnect has occurred by trying to allocate a slot when all are currently occupied
    score::cpp::ignore = unit_->AllocateNextSlot();

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then no slot is found
    EXPECT_FALSE(allocation.allocated_slot_index.has_value());
}

TEST_F(EventDataControlCompositeFixture, AsilBConsumerViolation)
{
    // Given an EventDataControlComposite with all slots ready
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics().WithARegisteredTransactionLog();
    AllocateAllSlots();
    ReadyAllSlots();

    // and an ASIL-B consumer, which blocks/references ALL slots
    auto upper_limit = EventSlotStatus::TIMESTAMP_MAX;
    for (auto counter = 0; counter < 5; ++counter)
    {
        auto slot_index = proxy_asil_local_->ReferenceNextEvent(0, *transaction_log_index_asil_, upper_limit);
        upper_limit = (*proxy_asil_local_)[slot_index.value()].GetTimeStamp();
    }

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then NO slot is found
    EXPECT_FALSE(allocation.allocated_slot_index.has_value());
}

/// Test is currently disabled as it violates a lola invariant that a given ProxyEvent instance should only
/// increment a single slot once (Ticket-130339). \todo Re-design and re-enable test (Ticket-128552)
TEST(EventDataControlCompositeTest, DISABLED_fuzz)
{
    RecordProperty("Verifies", "SSR-6225206");
    RecordProperty("Description", "Ensures correct slot allocation algorithm");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    memory::shared::NewDeleteDelegateMemoryResource memory{kMemoryResourceId};

    constexpr auto MAX_SLOTS = 100;
    constexpr auto MAX_SUBSCRIBERS = 10;
    EventDataControl asil{MAX_SLOTS, memory, MAX_SUBSCRIBERS};
    EventDataControl qm{MAX_SLOTS, memory, MAX_SUBSCRIBERS};
    SkeletonEventDataControlLocalView skeleton_asil_local{asil};
    SkeletonEventDataControlLocalView skeleton_qm_local{qm};

    EventDataControlComposite unit{skeleton_qm_local, &skeleton_asil_local, nullptr};

    std::mutex allocated_slots_mutex{};
    std::set<std::uint8_t> allocated_slots{};
    std::atomic<EventSlotStatus::EventTimeStamp> last_send_time_stamp{1};

    auto sender = [&allocated_slots, &allocated_slots_mutex, &unit, &last_send_time_stamp]() {
        for (auto counter = 0; counter < 100; counter++)
        {
            if (RandomTrueOrFalse())
            {
                // Allocate Slots
                const auto allocation = unit.AllocateNextSlot();
                if (allocation.allocated_slot_index.has_value())
                {
                    std::lock_guard<std::mutex> lock{allocated_slots_mutex};
                    allocated_slots.emplace(allocation.allocated_slot_index.value());
                }
            }
            else
            {
                // Free Slots
                std::unique_lock<std::mutex> lock{allocated_slots_mutex};
                if (!allocated_slots.empty())
                {
                    const auto index = RandomNumberBetween(0, allocated_slots.size() - 1);
                    auto iterator = allocated_slots.begin();
                    std::advance(iterator, index);
                    auto slot_index = *iterator;
                    score::cpp::ignore = allocated_slots.erase(iterator);
                    unit.EventReady(slot_index, last_send_time_stamp++);
                }
            }
        }
    };

    auto receiver = [&last_send_time_stamp, &qm, &asil]() {
        ProxyEventDataControlLocalView proxy_asil_local{asil};
        ProxyEventDataControlLocalView proxy_qm_local{qm};
        std::set<SlotIndexType> used_slots_qm{};
        std::set<SlotIndexType> used_slots_asil{};
        EventSlotStatus::EventTimeStamp start_ts{1};

        TransactionLogSet::TransactionLogIndex transaction_log_index_qm =
            proxy_qm_local.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogIdQm).value();
        TransactionLogSet::TransactionLogIndex transaction_log_index_asil =
            proxy_asil_local.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogIdAsil).value();

        for (auto counter = 0; counter < 100; counter++)
        {
            start_ts = static_cast<std::uint16_t>(
                RandomNumberBetween(static_cast<std::size_t>(start_ts), last_send_time_stamp));

            if (RandomTrueOrFalse())
            {
                // QM List
                if (used_slots_qm.size() < 5 && RandomTrueOrFalse())
                {
                    auto slot = proxy_qm_local.ReferenceNextEvent(start_ts, transaction_log_index_qm);
                    if (slot.has_value())
                    {
                        score::cpp::ignore = used_slots_qm.emplace(slot.value());
                        start_ts = EventSlotStatus{proxy_qm_local[slot.value()]}.GetTimeStamp();
                    }
                }
                else
                {
                    if (!used_slots_qm.empty())
                    {
                        const auto index = RandomNumberBetween(0, used_slots_qm.size() - 1);
                        auto iterator = used_slots_qm.begin();
                        std::advance(iterator, index);
                        const auto value = *iterator;
                        score::cpp::ignore = used_slots_qm.erase(iterator);
                        proxy_qm_local.DereferenceEvent(value, transaction_log_index_qm);
                    }
                }
            }
            else
            {
                // ASIL List
                if (used_slots_asil.size() < 5 && RandomTrueOrFalse())
                {
                    auto slot = proxy_asil_local.ReferenceNextEvent(start_ts, transaction_log_index_asil);
                    if (slot.has_value())
                    {
                        score::cpp::ignore = used_slots_asil.emplace(slot.value());
                        start_ts = EventSlotStatus{proxy_asil_local[slot.value()]}.GetTimeStamp();
                    }
                }
                else
                {
                    if (!used_slots_asil.empty())
                    {
                        const auto index = RandomNumberBetween(0, used_slots_asil.size() - 1);
                        auto iterator = used_slots_asil.begin();
                        std::advance(iterator, index);
                        const auto value = *iterator;
                        score::cpp::ignore = used_slots_asil.erase(iterator);
                        proxy_asil_local.DereferenceEvent(value, transaction_log_index_asil);
                    }
                }
            }
        }
    };

    std::vector<std::thread> thread_pool{};
    for (auto counter = 0; counter < 10; counter++)
    {
        if (counter % 2 == 0)
        {
            // attention: We are adding here multiple concurrent senders for the same event, which we do not assure
            // in our API strictly speaking ... but for fuzzing it is fruitful.
            thread_pool.emplace_back(sender);
        }
        else
        {
            thread_pool.emplace_back(receiver);
        }
    }

    for (auto& thread : thread_pool)
    {
        thread.join();
    }
}

TEST_F(EventDataControlCompositeFixture, GetQmEventDataControl)
{
    // Given an EventDataControlComposite with ASIL-QM and ASIL-B controls
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // When getting the QM event data control
    auto& qm_event_data_control = unit_->GetQmEventDataControlLocal();
    // we can call a method on the returned control.
    qm_event_data_control.AllocateNextSlot();
}

TEST_F(EventDataControlCompositeFixture, GetAsilBEventDataControl)
{
    // Given an EventDataControlComposite with ASIL-QM and ASIL-B controls
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // When getting the ASIL-B event data control
    auto* const asil_b_event_data_control_local = unit_->GetAsilBEventDataControlLocal();
    // expect, that we have a value
    EXPECT_NE(asil_b_event_data_control_local, nullptr);
    // and expect, we can call a method on the returned control.
    asil_b_event_data_control_local->AllocateNextSlot();
}

TEST_F(EventDataControlCompositeFixture, GetEmptyAsilBEventDataControl)
{
    // Given an EventDataControlComposite with only ASIL-QM
    WithQmOnlyEventDataControl().WithEventDataControlCompositeUsingRealAtomics();

    // When getting the ASIL-B event data control
    auto* const asil_b_event_data_control = unit_->GetAsilBEventDataControlLocal();
    // expect, that we get no value
    EXPECT_EQ(asil_b_event_data_control, nullptr);
}

TEST_F(EventDataControlCompositeFixture, GetProxyEventDataControlLocalView)
{
    // Given an EventDataControlComposite constructed with a ProxyEventDataControlLocalView
    EventDataControl qm{kMaxSlots, memory_, kMaxSubscribers};
    SkeletonEventDataControlLocalView<> skeleton_qm_local{qm};
    ProxyEventDataControlLocalView<> proxy_qm_local{qm};
    EventDataControlComposite<> unit{skeleton_qm_local, &proxy_qm_local};

    // When getting the QM event data control
    auto& returned_proxy_qm_local = unit.GetProxyEventDataControlLocalView();

    // Then the same ProxyEventDataControlLocalView that was passed to the constructor is returned
    EXPECT_EQ(&proxy_qm_local, &returned_proxy_qm_local);
}

TEST_F(EventDataControlCompositeFixture, GetProxyEventDataControlLocalViewWithAsilB)
{
    // Given an EventDataControlComposite constructed with a ProxyEventDataControlLocalView
    EventDataControl qm{kMaxSlots, memory_, kMaxSubscribers};
    EventDataControl asil_b{kMaxSlots, memory_, kMaxSubscribers};
    SkeletonEventDataControlLocalView<> skeleton_qm_local{qm};
    SkeletonEventDataControlLocalView<> skeleton_asil_b_local{asil_b};
    ProxyEventDataControlLocalView<> proxy_qm_local{qm};
    EventDataControlComposite<> unit{skeleton_qm_local, &skeleton_asil_b_local, &proxy_qm_local};

    // When getting the ASIL-B event data control
    auto& returned_proxy_qm_local = unit.GetProxyEventDataControlLocalView();

    // Then the same ProxyEventDataControlLocalView that was passed to the constructor is returned
    EXPECT_EQ(&proxy_qm_local, &returned_proxy_qm_local);
}

TEST_F(EventDataControlCompositeFixture,
       CallingGetProxyEventDataControlLocalViewWhenCompositeNotConstructedWithOneTerminates)
{
    // Given an EventDataControlComposite constructed without a ProxyEventDataControlLocalView
    EventDataControl qm{kMaxSlots, memory_, kMaxSubscribers};
    SkeletonEventDataControlLocalView<> skeleton_qm_local{qm};
    EventDataControlComposite<> unit{skeleton_qm_local, nullptr};

    // When getting the ASIL-B event data control
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit.GetProxyEventDataControlLocalView(), ".*");
}

using EventDataControlCompositeGetTimestampFixture = EventDataControlCompositeFixture;
TEST_F(EventDataControlCompositeGetTimestampFixture, CanAllocateOneSlot)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // When allocating one slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the first slot is used
    EXPECT_EQ(allocation.allocated_slot_index.value(), 0);

    // And there was no indication of QM misbehaviour
    EXPECT_FALSE(allocation.qm_misbehaved);
}

TEST_F(EventDataControlCompositeGetTimestampFixture, GetEventSlotTimestampReturnsTimestampOfAllocatedSlot)
{
    // Given an EventDataControlComposite with a single allocated slot which is marked as ready
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();
    const auto allocation = unit_->AllocateNextSlot();
    const auto slot = allocation.allocated_slot_index.value();

    const EventSlotStatus::EventTimeStamp slot_timestamp{10U};
    unit_->EventReady(slot, slot_timestamp);

    // When retrieving the timestamp of the slot
    const auto actual_timestamp = unit_->GetEventSlotTimestamp(slot);

    // Then the returned value should be the same as the value that was passed to EventReady
    EXPECT_EQ(actual_timestamp, slot_timestamp);
}

TEST_F(EventDataControlCompositeGetTimestampFixture, CanRetrieveTimestampsAsilB)
{
    const EventSlotStatus::EventTimeStamp slot_timestamp_0{10U};
    const EventSlotStatus::EventTimeStamp slot_timestamp_1{11U};
    const EventSlotStatus::EventTimeStamp slot_timestamp_2{12U};
    const EventSlotStatus::EventTimeStamp slot_timestamp_4{13U};

    const EventSlotStatus::EventTimeStamp in_writing_slot_timestamp_3{0U};

    // Given an EventDataControlComposite which only contains both a QM and ASIL B EventDataControl
    WithQmAndAsilBEventDataControls().WithEventDataControlCompositeUsingRealAtomics();

    // When all slots are written at one time
    AllocateAllSlots();

    // And all except for 1 are marked as ready
    unit_->EventReady(SlotIndexType{0}, slot_timestamp_0);
    unit_->EventReady(SlotIndexType{1}, slot_timestamp_1);
    unit_->EventReady(SlotIndexType{2}, slot_timestamp_2);
    unit_->EventReady(SlotIndexType{4}, slot_timestamp_4);

    // When retrieving the timestamp of the slots
    const auto actual_timestamp_0 = unit_->GetEventSlotTimestamp(0);
    const auto actual_timestamp_1 = unit_->GetEventSlotTimestamp(1);
    const auto actual_timestamp_2 = unit_->GetEventSlotTimestamp(2);
    const auto actual_timestamp_3 = unit_->GetEventSlotTimestamp(3);
    const auto actual_timestamp_4 = unit_->GetEventSlotTimestamp(4);

    // Then the returned values of the slots that were marked as ready should be the same as the value that was
    // passed to EventReady
    EXPECT_EQ(actual_timestamp_0, slot_timestamp_0);
    EXPECT_EQ(actual_timestamp_1, slot_timestamp_1);
    EXPECT_EQ(actual_timestamp_2, slot_timestamp_2);
    EXPECT_EQ(actual_timestamp_4, slot_timestamp_4);

    // and the slot that was not marked as ready should have the "InWriting" timestamp i.e. 0
    EXPECT_EQ(actual_timestamp_3, in_writing_slot_timestamp_3);
}

TEST_F(EventDataControlCompositeGetTimestampFixture, CanRetrieveTimestampsAsilQM)
{
    // Given an EventDataControlComposite which only contains a QM EventDataControl
    WithQmOnlyEventDataControl().WithEventDataControlCompositeUsingRealAtomics();

    const EventSlotStatus::EventTimeStamp slot_timestamp_0{10U};
    const EventSlotStatus::EventTimeStamp slot_timestamp_1{11U};
    const EventSlotStatus::EventTimeStamp slot_timestamp_2{12U};
    const EventSlotStatus::EventTimeStamp slot_timestamp_4{13U};

    const EventSlotStatus::EventTimeStamp in_writing_slot_timestamp_3{0U};

    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    for (auto counter = 0U; counter < 5U; ++counter)
    {
        unit_->AllocateNextSlot();
    }

    unit_->EventReady(SlotIndexType{0}, slot_timestamp_0);
    unit_->EventReady(SlotIndexType{1}, slot_timestamp_1);
    unit_->EventReady(SlotIndexType{2}, slot_timestamp_2);
    unit_->EventReady(SlotIndexType{4}, slot_timestamp_4);

    // When retrieving the timestamp of the slots
    const auto actual_timestamp_0 = unit_->GetEventSlotTimestamp(0);
    const auto actual_timestamp_1 = unit_->GetEventSlotTimestamp(1);
    const auto actual_timestamp_2 = unit_->GetEventSlotTimestamp(2);
    const auto actual_timestamp_3 = unit_->GetEventSlotTimestamp(3);
    const auto actual_timestamp_4 = unit_->GetEventSlotTimestamp(4);

    // Then the returned values of the slots that were marked as ready should be the same as the value that was
    // passed to EventReady
    EXPECT_EQ(actual_timestamp_0, slot_timestamp_0);
    EXPECT_EQ(actual_timestamp_1, slot_timestamp_1);
    EXPECT_EQ(actual_timestamp_2, slot_timestamp_2);
    EXPECT_EQ(actual_timestamp_4, slot_timestamp_4);

    // and the slot that was not marked as ready should have the "InWriting" timestamp i.e. max value of uint32_t
    EXPECT_EQ(actual_timestamp_3, in_writing_slot_timestamp_3);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
