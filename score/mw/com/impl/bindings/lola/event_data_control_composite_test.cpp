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

#include "score/mw/com/impl/bindings/lola/event_data_control.h"
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
        qm_ = std::make_unique<EventDataControl>(kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers);
        asil_ = std::make_unique<EventDataControl>(kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers);
        return *this;
    }

    EventDataControlCompositeFixture& WithQmOnlyEventDataControl()
    {
        qm_ = std::make_unique<EventDataControl>(kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers);
        return *this;
    }

    EventDataControlCompositeFixture& WithRealEventDataControlComposite()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(qm_ != nullptr);

        auto* const asil_control = asil_ != nullptr ? asil_.get() : nullptr;
        unit_ = std::make_unique<EventDataControlComposite>(qm_.get(), asil_control);
        return *this;
    }

    EventDataControlCompositeFixture& WithMockedEventDataControlComposite()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(qm_ != nullptr);

        atomic_mock_ = std::make_unique<memory::shared::AtomicMock<EventSlotStatus::value_type>>();
        memory::shared::AtomicIndirectorMock<EventSlotStatus::value_type>::SetMockObject(atomic_mock_.get());

        auto* const asil_control = asil_ != nullptr ? asil_.get() : nullptr;
        unit_mock_ = std::make_unique<
            detail_event_data_control_composite::EventDataControlCompositeImpl<memory::shared::AtomicIndirectorMock>>(
            qm_.get(), asil_control);

        return *this;
    }

    EventDataControlCompositeFixture& WithARegisteredTransactionLog()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(qm_ != nullptr);
        transaction_log_index_qm_ = std::make_unique<TransactionLogSet::TransactionLogIndex>(
            qm_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogIdQm).value());

        if (asil_ != nullptr)
        {
            transaction_log_index_asil_ = std::make_unique<TransactionLogSet::TransactionLogIndex>(
                asil_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogIdAsil).value());
        }
        return *this;
    }

    void AllocateAllSlots()
    {
        for (auto counter = 0U; counter < kSlotCount; ++counter)
        {
            slot_indicators_[counter] = unit_->AllocateNextSlot();
        }
    }

    void ReadyAllSlots()
    {
        for (SlotIndexType counter = 0; counter < kSlotCount; ++counter)
        {
            unit_->EventReady(slot_indicators_[counter], counter + 1u);
        }
    }

    void ReadyAllQmSlotsOnly()
    {
        for (SlotIndexType counter = 0; counter < kSlotCount; ++counter)
        {
            auto slot_indicator = slot_indicators_[counter];
            qm_->EventReady({slot_indicator.GetIndex(), slot_indicator.GetSlotQM()}, counter + 1u);
        }
    }

    memory::shared::NewDeleteDelegateMemoryResource memory_{kMemoryResourceId};
    std::array<ControlSlotCompositeIndicator, kSlotCount> slot_indicators_{};

    std::unique_ptr<EventDataControl> asil_{nullptr};
    std::unique_ptr<EventDataControl> qm_{nullptr};
    std::unique_ptr<EventDataControlComposite> unit_{nullptr};

    std::unique_ptr<memory::shared::AtomicMock<EventSlotStatus::value_type>> atomic_mock_{nullptr};
    std::unique_ptr<
        detail_event_data_control_composite::EventDataControlCompositeImpl<memory::shared::AtomicIndirectorMock>>
        unit_mock_{nullptr};

    std::unique_ptr<TransactionLogSet::TransactionLogIndex> transaction_log_index_qm_{nullptr};
    std::unique_ptr<TransactionLogSet::TransactionLogIndex> transaction_log_index_asil_{nullptr};
};

TEST_F(EventDataControlCompositeFixture, CanCreateAndDestroyFixture) {}

TEST_F(EventDataControlCompositeFixture, CanAllocateOneSlot)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

    // When allocating one slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the first slot is used
    EXPECT_EQ(allocation.GetIndex(), 0);

    // And there was no indication of QM misbehaviour
    EXPECT_TRUE(allocation.IsValidQmAndAsilB());
}

TEST_F(EventDataControlCompositeFixture, GetLatestTimeStampReturnCorrectValue)
{
    // Given an EventDataControlComposite with 1 allocated slots that are set to ready
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();
    auto slot_indicator = unit_->AllocateNextSlot();
    const EventSlotStatus::EventTimeStamp time_stamp{2};
    unit_->EventReady(slot_indicator, time_stamp);

    // When acquiring the latest timestamp
    const auto time_stscore_future_cpp_obtained = unit_->GetLatestTimestamp();

    // Then the timestamp is equal to 2
    EXPECT_EQ(time_stscore_future_cpp_obtained, time_stamp);
}

TEST_F(EventDataControlCompositeFixture, CanAllocateMultipleSlots)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

    // Then it is possible to allocate 2 slots.
    const auto allocation_slot_1 = unit_->AllocateNextSlot();
    const SlotIndexType slot_1_index{0};
    ASSERT_EQ(allocation_slot_1.GetIndex(), slot_1_index);
    const auto allocation_slot_2 = unit_->AllocateNextSlot();
    const SlotIndexType slot_2_index{1};
    ASSERT_EQ(allocation_slot_2.GetIndex(), slot_2_index);
}

TEST_F(EventDataControlCompositeFixture, GetLatestTimeStampReturnCorrectValueIfOneSlotIsMarkedAsInvalid)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

    // When allocating 2 slots
    auto slot_indicator_1 = unit_->AllocateNextSlot();
    auto slot_indicator_2 = unit_->AllocateNextSlot();

    // and set slot 1 to ready
    EventSlotStatus::EventTimeStamp time_stamp{2};
    unit_->EventReady(slot_indicator_1, time_stamp);

    // and discarding slot 2
    unit_->Discard(slot_indicator_2);

    // Then the timestamp is equal to 2
    time_stamp = unit_->GetLatestTimestamp();
    EventSlotStatus::EventTimeStamp expected_time_stamp{2};
    EXPECT_EQ(time_stamp, expected_time_stamp);
}

TEST_F(EventDataControlCompositeFixture, GetLatestTimeStampReturnsDefaultValuesIfAllSlotAreInvalid)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

    // When allocating 2 slots
    auto slot_indicator_1 = unit_->AllocateNextSlot();
    auto slot_indicator_2 = unit_->AllocateNextSlot();

    // and discarding them
    unit_->Discard(slot_indicator_1);
    unit_->Discard(slot_indicator_2);

    // Then the timestamp equal to 1
    auto time_stamp = unit_->GetLatestTimestamp();
    EventSlotStatus::EventTimeStamp expected_time_stamp{1};
    EXPECT_EQ(time_stamp, expected_time_stamp);
}

TEST_F(EventDataControlCompositeFixture, GetLatestTimeStampReturnsDefaultValuesIfNoSlotHaveBeenAllocated)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

    // Then the timestamp equal to 1
    auto time_stamp = unit_->GetLatestTimestamp();
    EventSlotStatus::EventTimeStamp expected_time_stamp{1};
    EXPECT_EQ(time_stamp, expected_time_stamp);
}

TEST_F(EventDataControlCompositeFixture,
       GetLatestTimeStampReturnsDefaultValuesIfASlotHasBeenAllocatedButNotMarkedAsReady)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

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
    WithQmAndAsilBEventDataControls().WithMockedEventDataControlComposite();

    // and given that the operation to update the QM slot value fails max_multi_allocate_count times
    EXPECT_CALL(*atomic_mock_, compare_exchange_strong(_, _, _))
        .Times(max_multi_allocate_count)
        .WillRepeatedly(Return(false));

    // When allocating one slot
    const auto allocation = unit_mock_->AllocateNextSlot();

    // Then it tries to allocate the asil b slot again and the first slot is still used
    EXPECT_EQ(allocation.GetIndex(), 0);

    // And there is an indication of QM misbehaviour. I.e. allocation only contains a valid ASIL-B slot pointer, but no
    // QM slot pointer.
    EXPECT_TRUE(allocation.IsValidAsilB() && !allocation.IsValidQM());
}

TEST_F(EventDataControlCompositeFixture, FailingToLockAsilMultiSlotStillAllocatesAsilBSlot)
{
    ::testing::InSequence sequence{};
    constexpr std::size_t max_multi_allocate_count{100U};

    // Given an EventDataControlComposite which mocks atomic operations with zero used slots
    WithQmAndAsilBEventDataControls().WithMockedEventDataControlComposite();

    // Given the operation to update the QM slot value succeeds but the operation to update the asil B slot fails
    // max_multi_allocate_count times
    for (std::size_t i = 0; i < max_multi_allocate_count; ++i)
    {
        EXPECT_CALL(*atomic_mock_, compare_exchange_strong(_, _, _)).WillOnce(Return(true));
        EXPECT_CALL(*atomic_mock_, compare_exchange_strong(_, _, _)).WillOnce(Return(false));
    }

    // When allocating one slot
    const auto allocation = unit_mock_->AllocateNextSlot();

    // Then it tries to allocate the asil b slot again and the first slot is still used
    EXPECT_EQ(allocation.GetIndex(), 0);

    // And there is an indication of QM misbehaviour
    EXPECT_TRUE(allocation.IsValidAsilB() && !allocation.IsValidQM());
}

TEST_F(EventDataControlCompositeFixture, CanAllocateOneSlotOnlyForQm)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmOnlyEventDataControl().WithRealEventDataControlComposite();

    // When allocating one slot
    auto allocation = unit_->AllocateNextSlot();

    // Then the first slot is used
    EXPECT_EQ(allocation.GetIndex(), 0);
    // and the QM slot ptr is valid
    EXPECT_TRUE(allocation.IsValidQM());
    // and the QM slot can be accessed without assert.
    score::cpp::ignore = allocation.GetSlotQM();
}

TEST_F(EventDataControlCompositeFixture, CanAllocateOneSlotWhenAlreadyOneIsAllocated)
{
    // Given an EventDataControlComposite with only one used slot
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();
    score::cpp::ignore = unit_->AllocateNextSlot();

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the second slot is used
    EXPECT_EQ(allocation.GetIndex(), 1);
}

TEST_F(EventDataControlCompositeFixture, OverWritesOldestSample)
{
    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();
    AllocateAllSlots();
    unit_->EventReady(slot_indicators_[3], 1);

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was marked ready
    EXPECT_EQ(allocation.GetIndex(), 3);
}

TEST_F(EventDataControlCompositeFixture, OverWritesDiscardedEvent)
{
    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();
    AllocateAllSlots();
    unit_->Discard(slot_indicators_[3]);

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was marked ready
    EXPECT_EQ(allocation.GetIndex(), 3);
}

TEST_F(EventDataControlCompositeFixture, OverWritesOldestSampleOnlyForQm)
{
    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    WithQmOnlyEventDataControl().WithRealEventDataControlComposite();
    AllocateAllSlots();
    unit_->EventReady(slot_indicators_[3], 1);

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was marked ready
    EXPECT_EQ(allocation.GetIndex(), 3);
}

TEST_F(EventDataControlCompositeFixture, SkipsEventIfUsedInQmList)
{
    // Given an EventDataControlComposite with all slots written at one time, and only one unused and a registered
    // transaction log
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite().WithARegisteredTransactionLog();
    AllocateAllSlots();
    unit_->EventReady(slot_indicators_[2], 1);
    unit_->EventReady(slot_indicators_[4], 2);

    score::cpp::ignore = qm_->ReferenceNextEvent(0, *transaction_log_index_qm_);  // slot 4 is used in QM list

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was only unused
    EXPECT_EQ(allocation.GetIndex(), 2);
}

TEST_F(EventDataControlCompositeFixture, SkipsEventIfUsedInAsilList)
{
    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite().WithARegisteredTransactionLog();
    AllocateAllSlots();
    unit_->EventReady(slot_indicators_[2], 1);
    unit_->EventReady(slot_indicators_[4], 2);
    score::cpp::ignore = asil_->ReferenceNextEvent(0, *transaction_log_index_asil_);  // slot 4 is used in ASIL list

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the slot is allocated, which was only unused
    EXPECT_EQ(allocation.GetIndex(), 2);
}

TEST_F(EventDataControlCompositeFixture, ReturnsNoSlotIfAllUsed)
{
    // Given an EventDataControlComposite in which all slots are used
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();
    AllocateAllSlots();

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then no slot is found
    EXPECT_FALSE(allocation.IsValidQM());
    EXPECT_FALSE(allocation.IsValidAsilB());
}

TEST_F(EventDataControlCompositeFixture, ReturnsNoSlotIfAllUsedQMOnly)
{
    // Given an EventDataControlComposite containing only a QM EventDataControl in which all slots are used
    WithQmOnlyEventDataControl().WithRealEventDataControlComposite();
    AllocateAllSlots();

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then no slot is found
    EXPECT_FALSE(allocation.IsValidQM());
    EXPECT_FALSE(allocation.IsValidAsilB());
}

TEST_F(EventDataControlCompositeFixture, QmConsumerViolation)
{
    RecordProperty("Verifies", "SCR-5899299, SCR-5899292");
    RecordProperty("Description", "Checks whether a QM process is ignored if he is miss behaving");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an EventDataControlComposite with all slots ready
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite().WithARegisteredTransactionLog();
    AllocateAllSlots();
    ReadyAllSlots();
    ASSERT_FALSE(unit_->IsQmControlDisconnected());
    // and a QM consumer, which blocks/references ALL slots
    auto upper_limit = EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX;
    for (auto counter = 0; counter < 5; ++counter)
    {
        auto slot_index = qm_->ReferenceNextEvent(0, *transaction_log_index_qm_, upper_limit);
        upper_limit = (*qm_)[slot_index.value()].GetTimeStamp();
    }

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then still a slot could be allocated for ASIL-B
    EXPECT_TRUE(allocation.IsValidAsilB());
    // but not for QM
    EXPECT_FALSE(allocation.IsValidQM());
}

TEST_F(EventDataControlCompositeFixture, AllocationIgnoresQMAfterContractViolation)
{
    RecordProperty("Verifies", "SCR-5899299, SCR-5899292");
    RecordProperty("Description", "Checks whether a QM process is ignored if he is miss behaving");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an EventDataControlComposite with all slots ready
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite().WithARegisteredTransactionLog();
    AllocateAllSlots();
    ReadyAllSlots();

    // and a QM consumer, which blocks/references ALL slots and thus already violated the contract
    auto upper_limit = EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX;
    for (auto counter = 0; counter < 5; ++counter)
    {
        auto slot_index = qm_->ReferenceNextEvent(0, *transaction_log_index_qm_, upper_limit);
        upper_limit = (*qm_)[slot_index.value()].GetTimeStamp();
    }
    score::cpp::ignore = unit_->AllocateNextSlot();
    ASSERT_TRUE(unit_->IsQmControlDisconnected());

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then still a slot could be allocated for ASIL-B
    EXPECT_TRUE(allocation.IsValidAsilB());
}

TEST_F(EventDataControlCompositeFixture, ReturnsNoSlotIfAllUsedAfterQmDisconnect)
{
    // Given an EventDataControlComposite containing only a QM EventDataControl in which all slots are used
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();
    AllocateAllSlots();

    // and given that QmControlDisconnect has occurred by trying to allocate a slot when all are currently occupied
    score::cpp::ignore = unit_->AllocateNextSlot();

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then no slot is found
    EXPECT_FALSE(allocation.IsValidQM());
    EXPECT_FALSE(allocation.IsValidAsilB());
}

TEST_F(EventDataControlCompositeFixture, AsilBConsumerViolation)
{
    // Given an EventDataControlComposite with all slots ready
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite().WithARegisteredTransactionLog();
    AllocateAllSlots();
    ReadyAllSlots();

    // and an ASIL-B consumer, which blocks/references ALL slots
    auto upper_limit = EventSlotStatus::TIMESTSCORE_LANGUAGE_FUTURECPP_MAX;
    for (auto counter = 0; counter < 5; ++counter)
    {
        auto slotindex = asil_->ReferenceNextEvent(0, *transaction_log_index_asil_, upper_limit);
        upper_limit = (*asil_)[slotindex.value()].GetTimeStamp();
    }

    // When allocating one additional slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then NO slot is found
    EXPECT_FALSE(allocation.IsValidQmAndAsilB());
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
    EventDataControl asil{MAX_SLOTS, memory.getMemoryResourceProxy(), MAX_SUBSCRIBERS};
    EventDataControl qm{MAX_SLOTS, memory.getMemoryResourceProxy(), MAX_SUBSCRIBERS};
    EventDataControlComposite unit{&qm, &asil};

    std::mutex allocated_slots_mutex{};
    std::vector<ControlSlotCompositeIndicator> allocated_slots{};
    std::atomic<EventSlotStatus::EventTimeStamp> last_send_time_stamp{1};

    auto sender = [&allocated_slots, &allocated_slots_mutex, &unit, &last_send_time_stamp]() {
        for (auto counter = 0; counter < 100; counter++)
        {
            if (RandomTrueOrFalse())
            {
                // Allocate Slots
                const auto allocation = unit.AllocateNextSlot();
                if (allocation.IsValidQmAndAsilB())
                {
                    std::lock_guard<std::mutex> lock{allocated_slots_mutex};
                    allocated_slots.push_back(allocation);
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
                    auto slot_indicator = *iterator;
                    score::cpp::ignore = allocated_slots.erase(iterator);
                    unit.EventReady(slot_indicator, last_send_time_stamp++);
                }
            }
        }
    };

    auto receiver = [&last_send_time_stamp, &qm, &asil]() {
        std::set<SlotIndexType> used_slots_qm{};
        std::set<SlotIndexType> used_slots_asil{};
        EventSlotStatus::EventTimeStamp start_ts{1};

        TransactionLogSet::TransactionLogIndex transaction_log_index_qm =
            qm.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogIdQm).value();
        TransactionLogSet::TransactionLogIndex transaction_log_index_asil =
            asil.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogIdAsil).value();

        for (auto counter = 0; counter < 100; counter++)
        {
            start_ts = static_cast<std::uint16_t>(
                RandomNumberBetween(static_cast<std::size_t>(start_ts), last_send_time_stamp));

            if (RandomTrueOrFalse())
            {
                // QM List
                if (used_slots_qm.size() < 5 && RandomTrueOrFalse())
                {
                    auto slot = qm.ReferenceNextEvent(start_ts, transaction_log_index_qm);
                    if (slot.has_value())
                    {
                        score::cpp::ignore = used_slots_qm.emplace(slot.value());
                        start_ts = EventSlotStatus{qm[slot.value()]}.GetTimeStamp();
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
                        qm.DereferenceEvent(value, transaction_log_index_qm);
                    }
                }
            }
            else
            {
                // ASIL List
                if (used_slots_asil.size() < 5 && RandomTrueOrFalse())
                {
                    auto slot = asil.ReferenceNextEvent(start_ts, transaction_log_index_asil);
                    if (slot.has_value())
                    {
                        score::cpp::ignore = used_slots_asil.emplace(slot.value());
                        start_ts = EventSlotStatus{qm[slot.value()]}.GetTimeStamp();
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
                        asil.DereferenceEvent(value, transaction_log_index_asil);
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
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

    // When getting the QM event data control
    auto& qm_event_data_control = unit_->GetQmEventDataControl();
    // we can call a method on the returned control.
    qm_event_data_control.GetNumNewEvents(0);
}

TEST_F(EventDataControlCompositeFixture, GetAsilBEventDataControl)
{
    // Given an EventDataControlComposite with ASIL-QM and ASIL-B controls
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

    // When getting the ASIL-B event data control
    const auto asil_b__event_data_control = unit_->GetAsilBEventDataControl();
    // expect, that we have a value
    EXPECT_TRUE(asil_b__event_data_control.has_value());
    // and expect, we can call a method on the returned control.
    asil_b__event_data_control.value()->GetNumNewEvents(0);
}

TEST_F(EventDataControlCompositeFixture, GetEmptyAsilBEventDataControl)
{
    // Given an EventDataControlComposite with only ASIL-QM
    WithQmOnlyEventDataControl().WithRealEventDataControlComposite();

    // When getting the ASIL-B event data control
    const auto asil_b__event_data_control = unit_->GetAsilBEventDataControl();
    // expect, that we get no value
    EXPECT_FALSE(asil_b__event_data_control.has_value());
}

TEST(EventDataControlCompositeDeathTest, DiesOnConstructionWithNullptr)
{
    EXPECT_DEATH(EventDataControlComposite(nullptr, nullptr), ".*");
}

using EventDataControlCompositeGetTimestampFixture = EventDataControlCompositeFixture;
TEST_F(EventDataControlCompositeGetTimestampFixture, CanAllocateOneSlot)
{
    // Given an EventDataControlComposite with zero used slots
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

    // When allocating one slot
    const auto allocation = unit_->AllocateNextSlot();

    // Then the first slot is used
    ASSERT_TRUE(allocation.IsValidQmAndAsilB());
    const auto slot = allocation.GetIndex();
    EXPECT_EQ(slot, 0);

    // And there was no indication of QM misbehaviour
    EXPECT_TRUE(allocation.IsValidQM());
}

TEST_F(EventDataControlCompositeGetTimestampFixture, GetEventSlotTimestampReturnsTimestampOfAllocatedSlot)
{
    // Given an EventDataControlComposite with a single allocated slot which is marked as ready
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();
    const auto allocation = unit_->AllocateNextSlot();
    const auto slot = allocation.GetIndex();

    const EventSlotStatus::EventTimeStamp slot_timestamp{10U};
    unit_->EventReady(allocation, slot_timestamp);

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
    WithQmAndAsilBEventDataControls().WithRealEventDataControlComposite();

    // When all slots are written at one time
    AllocateAllSlots();

    // And all except for 1 are marked as ready
    unit_->EventReady(slot_indicators_[0], slot_timestamp_0);
    unit_->EventReady(slot_indicators_[1], slot_timestamp_1);
    unit_->EventReady(slot_indicators_[2], slot_timestamp_2);
    unit_->EventReady(slot_indicators_[4], slot_timestamp_4);

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
    WithQmOnlyEventDataControl().WithRealEventDataControlComposite();

    const EventSlotStatus::EventTimeStamp slot_timestamp_0{10U};
    const EventSlotStatus::EventTimeStamp slot_timestamp_1{11U};
    const EventSlotStatus::EventTimeStamp slot_timestamp_2{12U};
    const EventSlotStatus::EventTimeStamp slot_timestamp_4{13U};

    const EventSlotStatus::EventTimeStamp in_writing_slot_timestamp_3{0U};

    // Given an EventDataControlComposite with all slots written at one time, and only one unused
    std::array<ControlSlotCompositeIndicator, 5U> slot_indicators{};
    for (auto counter = 0U; counter < 5U; ++counter)
    {
        slot_indicators[counter] = unit_->AllocateNextSlot();
    }

    unit_->EventReady(slot_indicators[0], slot_timestamp_0);
    unit_->EventReady(slot_indicators[1], slot_timestamp_1);
    unit_->EventReady(slot_indicators[2], slot_timestamp_2);
    unit_->EventReady(slot_indicators[4], slot_timestamp_4);

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
