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

#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"
#include "score/mw/com/impl/instance_specifier.h"

#include <gtest/gtest.h>

#include <utility>

namespace score::mw::com::impl::lola
{
namespace
{

struct DummyStruct
{
    std::uint8_t member1_;
    std::uint8_t member2_;
};

constexpr std::size_t kMaxSlots{5U};
constexpr std::size_t kMaxSubscribers{5U};
const TransactionLogId kDummyTransactionLogId{10U};

/// \brief Test fixture for SamplePtr functionality that only works for non-void types
class SamplePtrTest : public ::testing::Test
{
  protected:
    FakeMemoryResource memory_{};
    EventDataControl event_data_control_{kMaxSlots, memory_.getMemoryResourceProxy(), kMaxSubscribers};
    TransactionLogSet::TransactionLogIndex transaction_log_index_ =
        event_data_control_.GetTransactionLogSet().RegisterProxyElement(kDummyTransactionLogId).value();

    SlotIndexType AllocateSlot(EventSlotStatus::EventTimeStamp timestamp = 1)
    {
        auto slot = event_data_control_.AllocateNextSlot();
        EXPECT_TRUE(slot.IsValid());
        event_data_control_.EventReady(slot, timestamp);
        return slot.GetIndex();
    }
};

/// \brief Templated test fixture for SamplePtr functionality that works for both void and non-void types
///
/// \tparam SampleType The data type that is managed by the SamplePtr. Can either be a real type or void. The template
/// parameter can be accessed in the tests through TypeParam.
template <typename SampleType>
class SamplePtrGenericTypeTest : public SamplePtrTest
{
};

// Gtest will run all tests in the SamplePtrGenericTypeTest once for every type, t, in MyTypes, such that TypeParam == t
// for each run.
using MyTypes = ::testing::Types<uint8_t, void>;
TYPED_TEST_SUITE(SamplePtrGenericTypeTest, MyTypes, );

TYPED_TEST(SamplePtrGenericTypeTest, DereferencesAssignedSlot)
{
    auto slot_index = SamplePtrTest::AllocateSlot();

    auto client_slot_indicator =
        SamplePtrTest::event_data_control_.ReferenceNextEvent(0, SamplePtrTest::transaction_log_index_);
    ASSERT_TRUE(client_slot_indicator.IsValid());
    uint8_t dummy_val{};
    SamplePtr<TypeParam> sample_ptr{
        &dummy_val, SamplePtrTest::event_data_control_, client_slot_indicator, SamplePtrTest::transaction_log_index_};

    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot_index]}.GetReferenceCount(), 1);
    sample_ptr = nullptr;
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot_index]}.GetReferenceCount(), 0);
}

TYPED_TEST(SamplePtrGenericTypeTest, ProperMoveConstruction)
{
    auto slot_index = SamplePtrTest::AllocateSlot();

    auto client_slot_indicator =
        SamplePtrTest::event_data_control_.ReferenceNextEvent(0, SamplePtrTest::transaction_log_index_);
    ASSERT_TRUE(client_slot_indicator.IsValid());
    uint8_t dummy_val{};
    SamplePtr<TypeParam> sample_ptr{
        &dummy_val, SamplePtrTest::event_data_control_, client_slot_indicator, SamplePtrTest::transaction_log_index_};

    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot_index]}.GetReferenceCount(), 1);
    SamplePtr<TypeParam> another_sample_ptr{std::move(sample_ptr)};
    sample_ptr = nullptr;
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot_index]}.GetReferenceCount(), 1);
    another_sample_ptr = nullptr;
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot_index]}.GetReferenceCount(), 0);
}

TYPED_TEST(SamplePtrGenericTypeTest, ProperMoveAssignment)
{
    auto slot = SamplePtrTest::AllocateSlot(1);

    auto client_slot_indicator =
        SamplePtrTest::event_data_control_.ReferenceNextEvent(0, SamplePtrTest::transaction_log_index_);
    ASSERT_TRUE(client_slot_indicator.IsValid());
    uint8_t dummy_val{};
    SamplePtr<TypeParam> sample_ptr{
        &dummy_val, SamplePtrTest::event_data_control_, client_slot_indicator, SamplePtrTest::transaction_log_index_};

    auto slot2 = SamplePtrTest::AllocateSlot(2);

    auto client_slot2_indicator =
        SamplePtrTest::event_data_control_.ReferenceNextEvent(1, SamplePtrTest::transaction_log_index_);
    ASSERT_TRUE(client_slot2_indicator.IsValid());
    SamplePtr<TypeParam> sample_ptr2{
        &dummy_val, SamplePtrTest::event_data_control_, client_slot2_indicator, SamplePtrTest::transaction_log_index_};

    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot]}.GetReferenceCount(), 1);
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot2]}.GetReferenceCount(), 1);
    sample_ptr2 = std::move(sample_ptr);
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot]}.GetReferenceCount(), 1);
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot2]}.GetReferenceCount(), 0);
    sample_ptr2 = nullptr;
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot]}.GetReferenceCount(), 0);
    EXPECT_EQ(EventSlotStatus{SamplePtrTest::event_data_control_[slot2]}.GetReferenceCount(), 0);
}

TYPED_TEST(SamplePtrGenericTypeTest, TestStaticProperties)
{
    static_assert(!std::is_copy_constructible<SamplePtr<TypeParam>>::value,
                  "SamplePtr must not be copied to ensure proper reference counting");
    static_assert(!std::is_copy_assignable<SamplePtr<TypeParam>>::value,
                  "SamplePtr must not be copied to ensure proper reference counting");
}

TYPED_TEST(SamplePtrGenericTypeTest, ConstructFromNullptr)
{
    // Given a SamplePtr constructed from nullptr
    SamplePtr<TypeParam> sample_ptr{nullptr};

    // expect that bool op returns false
    EXPECT_FALSE(sample_ptr);
}

TEST_F(SamplePtrTest, ArrayOp)
{
    // Given an SamplePtr on an allocated slot
    AllocateSlot();
    auto client_slot_indicator = event_data_control_.ReferenceNextEvent(0, transaction_log_index_);
    ASSERT_TRUE(client_slot_indicator.IsValid());
    DummyStruct dummy_val{22, 44};
    SamplePtr<DummyStruct> sample_ptr{&dummy_val, event_data_control_, client_slot_indicator, transaction_log_index_};

    // When accessing the data via ->
    auto val1 = sample_ptr->member1_;
    auto val2 = sample_ptr->member2_;

    // Then the values are as expected
    EXPECT_EQ(val1, 22);
    EXPECT_EQ(val2, 44);
}

TEST_F(SamplePtrTest, StarOp)
{
    // Given an SamplePtr on an allocated slot
    AllocateSlot();
    auto client_slot_indicator = event_data_control_.ReferenceNextEvent(0, transaction_log_index_);
    ASSERT_TRUE(client_slot_indicator.IsValid());
    DummyStruct dummy_val{22, 44};
    SamplePtr<DummyStruct> sample_ptr{&dummy_val, event_data_control_, client_slot_indicator, transaction_log_index_};

    // When accessing the data via *
    auto val1 = *sample_ptr;

    // Then the values are as expected
    EXPECT_EQ(val1.member1_, 22);
    EXPECT_EQ(val1.member2_, 44);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
