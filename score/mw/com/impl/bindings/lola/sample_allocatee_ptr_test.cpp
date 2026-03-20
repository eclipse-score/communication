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
#include "score/mw/com/impl/bindings/lola/sample_allocatee_ptr.h"

#include "score/mw/com/impl/bindings/lola/skeleton_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>

#include <limits>
#include <type_traits>
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

class SampleAllocateePtrFixture : public ::testing::Test
{
  public:
    FakeMemoryResource memory_{};
    EventDataControl control_block_{kMaxSlots, memory_, kMaxSubscribers};
    SkeletonEventDataControlLocalView<> skeleton_event_data_control_local_{control_block_};
    EventDataControlComposite<> control_composite_{skeleton_event_data_control_local_, nullptr, nullptr};
};

TEST_F(SampleAllocateePtrFixture, MarksSlotAsInvalidOnDestruction)
{
    RecordProperty("Verifies", "SCR-6244646");
    RecordProperty(
        "Description",
        "SampleAllocateePtr shall free resources only on destruction. Note. the underlying memory of the pointed-to "
        "object is not deleted. Rather, it merely marks the slot as invalid and stops pointing to the object.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an SampleAllocateePtr on an allocated slot
    auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    std::uint8_t data{};
    {
        auto unit = SampleAllocateePtr<std::uint8_t>(&data, control_composite_, slot.value());
    }
    // When it goes out of scope

    // Then the underlying slot is marked invalid
    EXPECT_TRUE(skeleton_event_data_control_local_[slot.value()].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, DoesNotMarkSlotAsInvalidOnMove)
{
    // Given an SampleAllocateePtr on an allocated slot
    auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    std::uint8_t data{};
    auto unit = SampleAllocateePtr<std::uint8_t>(&data, control_composite_, slot.value());

    // When moving it
    auto unit2 = std::move(unit);

    // Then the underlying slot is _not_ marked invalid
    EXPECT_FALSE(skeleton_event_data_control_local_[slot.value()].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, ReadySlotIsNotMarkedInvalidOnDestruction)
{
    // Given an SampleAllocateePtr on an allocated slot that is already marked as ready
    auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    skeleton_event_data_control_local_.EventReady(slot.value(), 0x42);
    std::uint8_t data{};
    {
        auto unit = SampleAllocateePtr<std::uint8_t>(&data, control_composite_, slot.value());
    }
    // When it goes out of scope

    // Then the underlying slot is _not_ marked invalid
    EXPECT_FALSE(skeleton_event_data_control_local_[slot.value()].IsInvalid());
    EXPECT_EQ(skeleton_event_data_control_local_[slot.value()].GetTimeStamp(), 0x42);
}

TEST_F(SampleAllocateePtrFixture, CanAccessUnderlyingSlot)
{
    RecordProperty("Verifies", "SCR-6367235");
    RecordProperty("Description", "A valid SampleAllocateePtr and SamplePtr shall reference a valid and correct slot.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an SampleAllocateePtr on an allocated slot that is already marked as ready
    auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    skeleton_event_data_control_local_.EventReady(slot.value(), 0x42);
    std::uint8_t data{};
    auto unit = SampleAllocateePtr<std::uint8_t>(&data, control_composite_, slot.value());

    // When accessing which slot is associated with the SampleAllocateePtr
    auto referenced_slot = unit.GetReferencedSlot();

    // Then the underlying slot is the expected one and is valid
    EXPECT_EQ(referenced_slot, slot.value());
    EXPECT_FALSE(skeleton_event_data_control_local_[referenced_slot].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, ObeysOwnershipProperties)
{
    static_assert(std::is_move_constructible<SampleAllocateePtr<std::uint8_t>>::value, "Is not move constructable");
    static_assert(std::is_move_assignable<SampleAllocateePtr<std::uint8_t>>::value, "Is not move assignable");
    static_assert(!std::is_copy_constructible<SampleAllocateePtr<std::uint8_t>>::value, "Is copy constructable");
    static_assert(!std::is_copy_assignable<SampleAllocateePtr<std::uint8_t>>::value, "Is copy assignable");
}

TEST_F(SampleAllocateePtrFixture, MoveConstruct)
{
    // Given an SampleAllocateePtr on an allocated slot that is already marked as ready
    auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    skeleton_event_data_control_local_.EventReady(slot.value(), 0x42);
    std::uint8_t data{};
    auto unit = SampleAllocateePtr<std::uint8_t>(&data, control_composite_, slot.value());

    // When move constructing another SampleAllocateePtr from it
    SampleAllocateePtr<std::uint8_t> unit2(std::move(unit));

    // Then the move constructed instance contains the original members
    EXPECT_EQ(unit2.GetReferencedSlot(), slot.value());
    EXPECT_TRUE(unit2);

    // ... and the underlying slot is still valid.
    EXPECT_FALSE(skeleton_event_data_control_local_[slot.value()].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, MoveAssign)
{
    // Given an SampleAllocateePtr on an allocated slot that is already marked as ready
    auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    skeleton_event_data_control_local_.EventReady(slot.value(), 0x42);
    std::uint8_t data{};
    auto unit = SampleAllocateePtr<std::uint8_t>(&data, control_composite_, slot.value());

    // When move assigning to another SampleAllocateePtr
    SampleAllocateePtr<std::uint8_t> unit2 = std::move(unit);

    // Then the move constructed instance contains the original members
    EXPECT_EQ(unit2.GetReferencedSlot(), slot.value());
    EXPECT_TRUE(unit2);

    // ... and the underlying slot is still valid.
    EXPECT_FALSE(skeleton_event_data_control_local_[slot.value()].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, ConstructFromNullptr)
{
    // Given a SampleAllocateePtr constructed from nullptr
    auto unit = SampleAllocateePtr<std::uint8_t>(nullptr);

    // expect that ...
    EXPECT_FALSE(unit);
    EXPECT_EQ(unit.GetReferencedSlot(), std::numeric_limits<SlotIndexType>::max());
    EXPECT_EQ(unit.get(), nullptr);
}

TEST_F(SampleAllocateePtrFixture, AssignNullptr)
{
    // Given an SampleAllocateePtr on an allocated slot
    auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    std::uint8_t data{};
    auto unit = SampleAllocateePtr<std::uint8_t>(&data, control_composite_, slot.value());

    // When assigning a nullptr to it
    unit = nullptr;

    // Then the underlying slot is marked invalid
    EXPECT_TRUE(skeleton_event_data_control_local_[slot.value()].IsInvalid());
    // and the SamplePtr doesn't hold a valid managed object.
    EXPECT_FALSE(unit);
}

TEST_F(SampleAllocateePtrFixture, ArrayOp)
{
    // Given an SampleAllocateePtr on an allocated slot
    auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    DummyStruct data{99, 42};
    auto unit = SampleAllocateePtr<DummyStruct>(&data, control_composite_, slot.value());

    // When accessing the data via ->
    auto val1 = unit->member1_;
    auto val2 = unit->member2_;

    // Then the values are as expected
    EXPECT_EQ(val1, 99);
    EXPECT_EQ(val2, 42);
}

TEST_F(SampleAllocateePtrFixture, StarOp)
{
    // Given an SampleAllocateePtr on an allocated slot
    auto slot = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    DummyStruct data{99, 42};
    auto unit = SampleAllocateePtr<DummyStruct>(&data, control_composite_, slot.value());

    // When accessing the data via *
    auto val1 = *unit;

    // Then the values are as expected
    EXPECT_EQ(val1.member1_, 99);
    EXPECT_EQ(val1.member2_, 42);
}

TEST_F(SampleAllocateePtrFixture, SwapOp)
{
    // Given two SampleAllocateePtrs on allocated slots
    auto slot1 = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot1.has_value());
    DummyStruct data1{99, 42};
    auto unit1 = SampleAllocateePtr<DummyStruct>(&data1, control_composite_, slot1.value());

    auto slot2 = skeleton_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot2.has_value());
    DummyStruct data2{10, 100};
    auto unit2 = SampleAllocateePtr<DummyStruct>(&data2, control_composite_, slot2.value());

    // When swapping the SampleAllocateePtrs
    swap(unit1, unit2);

    // Then the values are swapped
    auto val1 = *unit1;
    auto val2 = *unit2;
    EXPECT_EQ(val1.member1_, data2.member1_);
    EXPECT_EQ(val1.member2_, data2.member2_);

    EXPECT_EQ(val2.member1_, data1.member1_);
    EXPECT_EQ(val2.member2_, data1.member2_);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
