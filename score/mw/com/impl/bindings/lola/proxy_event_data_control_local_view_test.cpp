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
#include "score/mw/com/impl/bindings/lola/proxy_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/event_slot_status.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include "score/memory/shared/atomic_indirector.h"
#include "score/memory/shared/atomic_mock.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>
#include <chrono>
#include <limits>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace score::mw::com::impl::lola
{

namespace
{

using ::testing::_;
using ::testing::Return;

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

class ProxyEventDataControlLocalViewFixture : public ::testing::Test
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

    ProxyEventDataControlLocalViewFixture& GivenARealProxyEventDataControlLocalView(
        const SlotIndexType max_slots,
        const LolaEventInstanceDeployment::SubscriberCountType max_subscribers)
    {
        event_data_control_ = std::make_unique<EventDataControl>(max_slots, memory_, max_subscribers);
        unit_ = std::make_unique<ProxyEventDataControlLocalView<>>(*event_data_control_);
        skeleton_event_data_control_local_ =
            std::make_unique<SkeletonEventDataControlLocalView<>>(*event_data_control_);

        return *this;
    }

    ProxyEventDataControlLocalViewFixture& GivenAMockedEventDataControl(
        const SlotIndexType max_slots,
        const LolaEventInstanceDeployment::SubscriberCountType max_subscribers)
    {
        event_data_control_ = std::make_unique<EventDataControl>(max_slots, memory_, max_subscribers);

        atomic_mock_ = std::make_unique<memory::shared::AtomicMock<EventSlotStatus::value_type>>();
        memory::shared::AtomicIndirectorMock<EventSlotStatus::value_type>::SetMockObject(atomic_mock_.get());

        unit_mock_ = std::make_unique<ProxyEventDataControlLocalView<memory::shared::AtomicIndirectorMock>>(
            *event_data_control_);
        skeleton_event_data_control_local_mocked_ =
            std::make_unique<SkeletonEventDataControlLocalView<memory::shared::AtomicIndirectorMock>>(
                *event_data_control_);

        return *this;
    }

    SlotIndexType WithAnAllocatedSlot(EventSlotStatus::EventTimeStamp timestamp = 1)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(skeleton_event_data_control_local_ != nullptr);
        auto slot = skeleton_event_data_control_local_->AllocateNextSlot();
        EXPECT_TRUE(slot.IsValid());
        skeleton_event_data_control_local_->EventReady(slot, timestamp);
        return slot.GetIndex();
    }

    FakeMemoryResource memory_{};
    std::unique_ptr<memory::shared::AtomicMock<EventSlotStatus::value_type>> atomic_mock_{nullptr};

    std::unique_ptr<EventDataControl> event_data_control_{nullptr};
    std::unique_ptr<SkeletonEventDataControlLocalView<>> skeleton_event_data_control_local_{nullptr};
    std::unique_ptr<SkeletonEventDataControlLocalView<memory::shared::AtomicIndirectorMock>>
        skeleton_event_data_control_local_mocked_{nullptr};
    std::unique_ptr<ProxyEventDataControlLocalView<>> unit_{nullptr};
    std::unique_ptr<ProxyEventDataControlLocalView<memory::shared::AtomicIndirectorMock>> unit_mock_{nullptr};
};

TEST_F(ProxyEventDataControlLocalViewFixture, RegisterProxyElementReturnsValidTransactionLogIndex)
{
    // Given a EventDataControlUnit
    GivenARealProxyEventDataControlLocalView(1, kMaxSubscribers);

    // When registering the proxy with the TransactionLogSet
    const auto transaction_log_index_result =
        unit_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId);

    // Then we get a valid transaction log index
    ASSERT_TRUE(transaction_log_index_result.has_value());
    EXPECT_EQ(transaction_log_index_result.value(), 0);
}

TEST_F(ProxyEventDataControlLocalViewFixture, FindNextSlotBlocksAllocation)
{
    // Given a EventDataControlUnit with one ready slot
    GivenARealProxyEventDataControlLocalView(1, kMaxSubscribers).WithAnAllocatedSlot(1);

    const auto transaction_log_index =
        unit_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When finding the next slot
    auto event = unit_->ReferenceNextEvent(0, transaction_log_index);
    ASSERT_TRUE(event.IsValid());

    // Then we cannot allocate the slot again
    ASSERT_FALSE(skeleton_event_data_control_local_->AllocateNextSlot().IsValid());
}

// Re-enable when the test is fixed in Ticket-128552
TEST_F(ProxyEventDataControlLocalViewFixture, DISABLED_MultipleReceiverRefCountCheck)
{
    const std::size_t max_subscribers{10U};

    // Given an EventDataControl with one ready slot
    GivenARealProxyEventDataControlLocalView(1, kMaxSubscribers).WithAnAllocatedSlot(1);

    auto receiver_tester = [this]() {
        const auto transaction_log_index =
            unit_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();
        for (auto counter = 0U; counter < 1000U; counter++)
        {
            // We can increase the ref-count
            auto receive_slot = unit_->ReferenceNextEvent(0, transaction_log_index, EventSlotStatus::TIMESTAMP_MAX);
            ASSERT_TRUE(receive_slot.IsValid());
            EXPECT_EQ((*unit_)[receive_slot.GetIndex()].GetTimeStamp(), 1);

            // and decrease the ref-count
            unit_->DereferenceEvent(receive_slot, transaction_log_index);
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
    ASSERT_TRUE(skeleton_event_data_control_local_->AllocateNextSlot().IsValid());
}

TEST_F(ProxyEventDataControlLocalViewFixture, FailingToUpdateSlotValueCausesReferenceNextEventToReturnNull)
{
    using namespace score::memory::shared;

    constexpr auto max_reference_retries{100U};

    GivenAMockedEventDataControl(1, kMaxSubscribers);

    // Given the operation to update the slot value fails max_reference_retries times
    EXPECT_CALL(*atomic_mock_, compare_exchange_weak(_, _, _))
        .Times(max_reference_retries)
        .WillRepeatedly(Return(false));

    // and a EventDataControlUnit with one ready slot
    auto slot = skeleton_event_data_control_local_mocked_->AllocateNextSlot();
    skeleton_event_data_control_local_mocked_->EventReady(slot, 1);

    const auto transaction_log_index =
        unit_mock_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When finding the next slot
    auto event = unit_mock_->ReferenceNextEvent(0, transaction_log_index);

    // No event will be found
    ASSERT_FALSE(event.IsValid());
}
using EventDataControlReferenceSpecificEventFixture = ProxyEventDataControlLocalViewFixture;
TEST_F(EventDataControlReferenceSpecificEventFixture, ReferenceSpecificEvents)
{
    const std::size_t max_number_slots{6U};
    const std::size_t subscription_slots{6U};

    // Given an EventDataControl with 6 ready slots
    GivenARealProxyEventDataControlLocalView(max_number_slots, kMaxSubscribers);
    for (unsigned int i = 0; i < 6; i++)
    {
        WithAnAllocatedSlot(i + 1);
    }

    const auto transaction_log_index =
        unit_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When explicitly referencing (ref-count-incrementing) each of them by index this is successful
    for (SlotIndexType i = 0; i < subscription_slots; i++)
    {
        EXPECT_EQ((*unit_)[i].GetReferenceCount(), 0U);
        unit_->ReferenceSpecificEvent(i, transaction_log_index);
        EXPECT_EQ((*unit_)[i].GetReferenceCount(), 1U);
    }
}

using EventDataControlReferenceSpecificEventDeathTest = EventDataControlReferenceSpecificEventFixture;
TEST_F(EventDataControlReferenceSpecificEventDeathTest, ReferenceSpecificEvent_StatusInvalidTerminates)
{
    // Given an EventDataControl with one (initially invalid) slot
    GivenARealProxyEventDataControlLocalView(1, kMaxSubscribers);
    EXPECT_TRUE((*unit_)[0].IsInvalid());

    const auto transaction_log_index =
        unit_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When explicitly referencing (ref-count-incrementing) it
    // Then the program terminates
    EXPECT_DEATH(unit_->ReferenceSpecificEvent(static_cast<SlotIndexType>(0), transaction_log_index), ".*");
}

TEST_F(EventDataControlReferenceSpecificEventDeathTest, ReferenceSpecificEvent_StatusInWritingTerminates)
{
    // Given an EventDataControl with one in_writing slot
    GivenARealProxyEventDataControlLocalView(1, kMaxSubscribers);
    auto slot = skeleton_event_data_control_local_->AllocateNextSlot();
    ASSERT_TRUE(slot.IsValid());
    EXPECT_TRUE((*unit_)[slot.GetIndex()].IsInWriting());

    const auto transaction_log_index =
        unit_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When explicitly referencing (ref-count-incrementing) it
    // Then the program terminates
    EXPECT_DEATH(unit_->ReferenceSpecificEvent(static_cast<SlotIndexType>(0), transaction_log_index), ".*");
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
    GivenAMockedEventDataControl(1, kMaxSubscribers);
    auto slot = skeleton_event_data_control_local_mocked_->AllocateNextSlot();
    ASSERT_TRUE(slot.IsValid());

    const auto transaction_log_index =
        unit_mock_->GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    // When explicitly referencing (ref-count-incrementing) it which would lead to the ref count overflowing
    // Then the program terminates
    EXPECT_DEATH(unit_mock_->ReferenceSpecificEvent(static_cast<SlotIndexType>(0), transaction_log_index), ".*");
}

TEST_F(ProxyEventDataControlLocalViewFixture, GetNumNewEvents_Zero)
{
    // Given an EventDataControl with one ready slot
    GivenARealProxyEventDataControlLocalView(1, kMaxSubscribers).WithAnAllocatedSlot(1);

    // When checking for new samples since timestamp 1 expect, that 0 is returned.
    EXPECT_EQ(unit_->GetNumNewEvents(1), 0);
}

TEST_F(ProxyEventDataControlLocalViewFixture, GetNumNewEvents_One)
{
    // Given an EventDataControl with one ready slot
    GivenARealProxyEventDataControlLocalView(1, kMaxSubscribers).WithAnAllocatedSlot(1);

    // When checking for new samples since start (timestamp 0) expect, that 1 is returned.
    EXPECT_EQ(unit_->GetNumNewEvents(0), 1);
}

TEST_F(ProxyEventDataControlLocalViewFixture, GetNumNewEvents_Many)
{
    // Given an EventDataControl with 6 ready slots
    GivenARealProxyEventDataControlLocalView(6, kMaxSubscribers);
    for (unsigned int i = 1; i <= 6; i++)
    {
        WithAnAllocatedSlot(i);
    }

    // When checking for new samples since timestamp 0 to 6 expect, that 6 up to 0 is returned.
    EXPECT_EQ(unit_->GetNumNewEvents(0), 6);
    EXPECT_EQ(unit_->GetNumNewEvents(1), 5);
    EXPECT_EQ(unit_->GetNumNewEvents(2), 4);
    EXPECT_EQ(unit_->GetNumNewEvents(3), 3);
    EXPECT_EQ(unit_->GetNumNewEvents(4), 2);
    EXPECT_EQ(unit_->GetNumNewEvents(5), 1);
    EXPECT_EQ(unit_->GetNumNewEvents(6), 0);
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
        ProxyEventDataControlLocalView<>::ResetPerformanceCounters();
    }
    ~MultiSenderMultiReceiverTest() override
    {
        ProxyEventDataControlLocalView<>::DumpPerformanceCounters();
    }

  protected:
    // Protected by lock_
    std::mutex lock_{};
    EventSlotStatus::EventTimeStamp next_ts_{1};
    std::set<SlotIndexType> allocated_slots_{};

    // Unprotected
    FakeMemoryResource memory_{};
    EventDataControl event_data_control_{GetParam().num_slots, memory_, GetParam().num_receiver_threads};
    SkeletonEventDataControlLocalView<> skeleton_event_data_control_local_{event_data_control_};
    ProxyEventDataControlLocalView<> unit_{event_data_control_};
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
            auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
            ASSERT_TRUE(slot.IsValid());

            skeleton_event_data_control_local_.EventReady(slot, ++ts);
        }
    };

    auto receiver = [this]() {
        // randomly increases or decreases ref-count
        const auto& params = GetParam();

        std::vector<ControlSlotIndicator> used_slots{};
        EventSlotStatus::EventTimeStamp start_ts{1};

        TransactionLogSet::TransactionLogIndex transaction_log_index =
            unit_.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

        for (std::size_t counter = 0; counter < params.num_actions_per_receiver; ++counter)
        {
            if (used_slots.size() < params.max_referenced_samples_per_receiver && RandomTrueOrFalse())
            {
                auto slot = unit_.ReferenceNextEvent(start_ts, transaction_log_index);
                if (slot.IsValid())
                {
                    start_ts = EventSlotStatus{unit_[slot.GetIndex()]}.GetTimeStamp();
                    used_slots.push_back(slot);
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
            auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
            ASSERT_NE(ts, std::numeric_limits<std::uint32_t>::max());
            if (!slot.IsValid())
            {
                ProxyEventDataControlLocalView<>::DumpPerformanceCounters();
                std::terminate();
            }
            else
            {
                skeleton_event_data_control_local_.EventReady(slot, ++ts);
            }
            EXPECT_TRUE(slot.IsValid());
        }
    };

    auto receiver = [this]() {
        // randomly increases or decreases ref-count
        const auto& params = GetParam();

        std::vector<ControlSlotIndicator> used_slots{};
        EventSlotStatus::EventTimeStamp start_ts{0};

        TransactionLogSet::TransactionLogIndex transaction_log_index =
            unit_.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

        for (std::size_t counter = 0; counter < params.num_actions_per_receiver; ++counter)
        {
            EventSlotStatus::EventTimeStamp highest_ts{EventSlotStatus::TIMESTAMP_MAX};
            std::optional<SlotIndexType> slot_last;
            std::size_t counter1 = 0;
            while (counter1 <= params.max_referenced_samples_per_receiver &&
                   used_slots.size() < params.max_referenced_samples_per_receiver)
            {
                auto slot = unit_.ReferenceNextEvent(start_ts, transaction_log_index, highest_ts);
                if (slot.IsValid())
                {
                    highest_ts = EventSlotStatus{unit_[slot.GetIndex()]}.GetTimeStamp();
                    used_slots.push_back(slot);
                    slot_last = slot.GetIndex();
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
