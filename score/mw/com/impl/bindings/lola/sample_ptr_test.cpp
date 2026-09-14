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
#include "score/mw/com/impl/bindings/lola/sample_ptr.h"

#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/provider_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

#include <utility>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr std::size_t kMaxSlots{5U};

/// \brief Test fixture for SamplePtr functionality that only works for non-void types
class SamplePtrTest : public ::testing::Test
{
  protected:
    FakeMemoryResource memory_{};
    EventDataControl event_data_control_{kMaxSlots, memory_};
    TransactionLog transaction_log_{kMaxSlots, memory_};
    ConsumerEventDataControlLocalView<> consumer_event_data_control_local_{event_data_control_, transaction_log_};
    ProviderEventDataControlLocalView<> provider_event_data_control_local_{event_data_control_};

    SlotIndexType AllocateSlot(EventSlotStatus::EventTimeStamp timestamp = 1)
    {
        auto slot = provider_event_data_control_local_.AllocateNextSlot();
        EXPECT_TRUE(slot.has_value());
        provider_event_data_control_local_.EventReady(slot.value(), timestamp);
        return slot.value();
    }

    SamplePtr CreateSamplePtr(const EventSlotStatus::EventTimeStamp timestamp,
                              const EventSlotStatus::EventTimeStamp last_search_time)
    {
        AllocateSlot(timestamp);
        auto slot_index = consumer_event_data_control_local_.ReferenceNextEvent(last_search_time);
        EXPECT_TRUE(slot_index.has_value());

        dummy_storage_.push_back(std::make_unique<std::uint8_t>(0U));
        return SamplePtr{dummy_storage_.back().get(), consumer_event_data_control_local_, slot_index.value()};
    }
    std::vector<std::unique_ptr<std::uint8_t>> dummy_storage_;
};

TEST_F(SamplePtrTest, DereferencesAssignedSlot)
{
    auto slot_index = SamplePtrTest::AllocateSlot();

    auto client_slot_result = SamplePtrTest::consumer_event_data_control_local_.ReferenceNextEvent(0);
    ASSERT_TRUE(client_slot_result.has_value());
    uint8_t dummy_val{};
    SamplePtr sample_ptr{&dummy_val, SamplePtrTest::consumer_event_data_control_local_, client_slot_result.value()};

    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot_index]}.GetReferenceCount(), 1);
    sample_ptr = nullptr;
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot_index]}.GetReferenceCount(), 0);
}

TEST_F(SamplePtrTest, ProperMoveConstruction)
{
    auto slot_index = SamplePtrTest::AllocateSlot();

    auto client_slot_result = SamplePtrTest::consumer_event_data_control_local_.ReferenceNextEvent(0);
    ASSERT_TRUE(client_slot_result.has_value());
    uint8_t dummy_val{};
    SamplePtr sample_ptr{&dummy_val, SamplePtrTest::consumer_event_data_control_local_, client_slot_result.value()};

    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot_index]}.GetReferenceCount(), 1);
    SamplePtr another_sample_ptr{std::move(sample_ptr)};
    sample_ptr = nullptr;
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot_index]}.GetReferenceCount(), 1);
    another_sample_ptr = nullptr;
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot_index]}.GetReferenceCount(), 0);
}

TEST_F(SamplePtrTest, ProperMoveAssignment)
{
    auto slot = SamplePtrTest::AllocateSlot(1);

    auto client_slot_result = SamplePtrTest::consumer_event_data_control_local_.ReferenceNextEvent(0);
    ASSERT_TRUE(client_slot_result.has_value());
    uint8_t dummy_val{};
    SamplePtr sample_ptr{&dummy_val, SamplePtrTest::consumer_event_data_control_local_, client_slot_result.value()};

    auto slot2 = SamplePtrTest::AllocateSlot(2);

    auto client_slot_result_2 = SamplePtrTest::consumer_event_data_control_local_.ReferenceNextEvent(1);
    ASSERT_TRUE(client_slot_result_2.has_value());
    SamplePtr sample_ptr2{&dummy_val, SamplePtrTest::consumer_event_data_control_local_, client_slot_result_2.value()};

    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot]}.GetReferenceCount(), 1);
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot2]}.GetReferenceCount(), 1);
    sample_ptr2 = std::move(sample_ptr);
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot]}.GetReferenceCount(), 1);
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot2]}.GetReferenceCount(), 0);
    sample_ptr2 = nullptr;
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot]}.GetReferenceCount(), 0);
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::consumer_event_data_control_local_[slot2]}.GetReferenceCount(), 0);
}

TEST_F(SamplePtrTest, TestStaticProperties)
{
    static_assert(!std::is_copy_constructible<SamplePtr>::value,
                  "SamplePtr must not be copied to ensure proper reference counting");
    static_assert(!std::is_copy_assignable<SamplePtr>::value,
                  "SamplePtr must not be copied to ensure proper reference counting");
}

TEST_F(SamplePtrTest, ConstructFromNullptr)
{
    // Given a SamplePtr constructed from nullptr
    SamplePtr sample_ptr{nullptr};

    // expect that bool op returns false
    EXPECT_FALSE(sample_ptr);
}

// Note: ArrayOp/StarOp tests (testing operator->/operator* dereferencing a typed slot value) used to live here when
// lola::SamplePtr was still a template. Since the binding layer's SamplePtr is now fully type-erased (const void*),
// typed dereferencing is no longer meaningful at this layer; it is performed by the binding-independent
// impl::SamplePtr<SampleType>, whose operator->/operator* are covered by
// score/mw/com/impl/plumbing/sample_ptr_test.cpp (CanDereference, CanArrowOperator).

TEST_F(SamplePtrTest, GreaterThanReturnsTrueWhenLeftSampleHasNewerTimestamp)
{
    constexpr EventSlotStatus::EventTimeStamp kOlderTimestamp{10U};
    constexpr EventSlotStatus::EventTimeStamp kNewerTimestamp{42U};

    auto older_sample = CreateSamplePtr(kOlderTimestamp, 0U);
    auto newer_sample = CreateSamplePtr(kNewerTimestamp, 1U);

    const bool result = newer_sample > older_sample;

    EXPECT_TRUE(result);
}

TEST_F(SamplePtrTest, GreaterThanReturnsFalseWhenLeftSampleHasOlderTimestamp)
{
    constexpr EventSlotStatus::EventTimeStamp kOlderTimestamp{10U};
    constexpr EventSlotStatus::EventTimeStamp kNewerTimestamp{42U};

    auto older_sample = CreateSamplePtr(kOlderTimestamp, 0U);
    auto newer_sample = CreateSamplePtr(kNewerTimestamp, 1U);

    const bool result = older_sample > newer_sample;

    EXPECT_FALSE(result);
}

TEST_F(SamplePtrTest, SortByTimestampOrdersSamplesFromNewestToOldest)
{
    constexpr EventSlotStatus::EventTimeStamp kOldestTimestamp{10U};
    constexpr EventSlotStatus::EventTimeStamp kMiddleTimestamp{42U};
    constexpr EventSlotStatus::EventTimeStamp kNewestTimestamp{43U};

    std::vector<SamplePtr> samples{};
    samples.emplace_back(CreateSamplePtr(kOldestTimestamp, 0U));
    samples.emplace_back(CreateSamplePtr(kMiddleTimestamp, 1U));
    samples.emplace_back(CreateSamplePtr(kNewestTimestamp, 2U));

    std::sort(samples.begin(), samples.end(), [](const auto& lhs, const auto& rhs) {
        return lhs > rhs;
    });

    EXPECT_TRUE(samples[0] > samples[1]);
    EXPECT_TRUE(samples[0] > samples[2]);
    EXPECT_TRUE(samples[1] > samples[2]);
    EXPECT_FALSE(samples[2] > samples[0]);
    EXPECT_FALSE(samples[2] > samples[1]);
}

TEST_F(SamplePtrTest, LessThanReturnsTrueWhenLeftSampleHasOlderTimestamp)
{
    constexpr EventSlotStatus::EventTimeStamp kOlderTimestamp{10U};
    constexpr EventSlotStatus::EventTimeStamp kNewerTimestamp{42U};

    auto older_sample = CreateSamplePtr(kOlderTimestamp, 0U);
    auto newer_sample = CreateSamplePtr(kNewerTimestamp, 1U);

    const bool result = older_sample < newer_sample;

    EXPECT_TRUE(result);
}

TEST_F(SamplePtrTest, LessThanReturnsFalseWhenLeftSampleHasNewerTimestamp)
{
    constexpr EventSlotStatus::EventTimeStamp kOlderTimestamp{10U};
    constexpr EventSlotStatus::EventTimeStamp kNewerTimestamp{42U};

    auto older_sample = CreateSamplePtr(kOlderTimestamp, 0U);
    auto newer_sample = CreateSamplePtr(kNewerTimestamp, 1U);

    const bool result = newer_sample < older_sample;

    EXPECT_FALSE(result);
}

TEST_F(SamplePtrTest, SortByTimestampOrdersSamplesFromOldestToNewest)
{
    constexpr EventSlotStatus::EventTimeStamp kOldestTimestamp{10U};
    constexpr EventSlotStatus::EventTimeStamp kMiddleTimestamp{42U};
    constexpr EventSlotStatus::EventTimeStamp kNewestTimestamp{43U};

    std::vector<SamplePtr> samples{};
    samples.emplace_back(CreateSamplePtr(kOldestTimestamp, 0U));
    samples.emplace_back(CreateSamplePtr(kMiddleTimestamp, 1U));
    samples.emplace_back(CreateSamplePtr(kNewestTimestamp, 2U));

    std::sort(samples.begin(), samples.end(), [](const auto& lhs, const auto& rhs) {
        return lhs < rhs;
    });

    EXPECT_TRUE(samples[0] < samples[1]);
    EXPECT_TRUE(samples[0] < samples[2]);
    EXPECT_TRUE(samples[1] < samples[2]);
    EXPECT_FALSE(samples[2] < samples[0]);
    EXPECT_FALSE(samples[2] < samples[1]);
}

TEST_F(SamplePtrTest, GreaterThanReturnsFalseWhenLeftSampleIsInvalid)
{
    constexpr EventSlotStatus::EventTimeStamp kTimestamp{42U};

    SamplePtr invalid_sample{};
    auto valid_sample = CreateSamplePtr(kTimestamp, 0U);

    const bool result = invalid_sample > valid_sample;

    EXPECT_FALSE(result);
}

TEST_F(SamplePtrTest, GreaterThanReturnsFalseWhenRightSampleIsInvalid)
{
    constexpr EventSlotStatus::EventTimeStamp kTimestamp{42U};

    auto valid_sample = CreateSamplePtr(kTimestamp, 0U);
    SamplePtr invalid_sample{};

    const bool result = valid_sample > invalid_sample;

    EXPECT_FALSE(result);
}

TEST_F(SamplePtrTest, LessThanReturnsFalseWhenLeftSampleIsInvalid)
{
    constexpr EventSlotStatus::EventTimeStamp kTimestamp{42U};

    SamplePtr invalid_sample{};
    auto valid_sample = CreateSamplePtr(kTimestamp, 0U);

    const bool result = invalid_sample < valid_sample;

    EXPECT_FALSE(result);
}

TEST_F(SamplePtrTest, LessThanReturnsFalseWhenRightSampleIsInvalid)
{
    constexpr EventSlotStatus::EventTimeStamp kTimestamp{42U};

    auto valid_sample = CreateSamplePtr(kTimestamp, 0U);
    SamplePtr invalid_sample{};

    const bool result = valid_sample < invalid_sample;

    EXPECT_FALSE(result);
}
}  // namespace
}  // namespace score::mw::com::impl::lola
