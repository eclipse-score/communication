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

#include "score/mw/com/impl/bindings/lola/provider_event_data_control_local_view.h"
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

class SampleAllocateePtrFixture : public ::testing::Test
{
  public:
    FakeMemoryResource memory_{};
    EventDataControl control_block_{kMaxSlots, memory_};
    ProviderEventDataControlLocalView<> provider_event_data_control_local_{control_block_};
    ConsumerEventDataControlLocalView<> consumer_event_data_control_local_{control_block_};
    EventDataControlComposite<> control_composite_{provider_event_data_control_local_, nullptr};
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
    auto slot = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    std::uint8_t data{};
    {
        auto unit = SampleAllocateePtr(&data, control_composite_, consumer_event_data_control_local_, slot.value());
    }
    // When it goes out of scope

    // Then the underlying slot is marked invalid
    EXPECT_TRUE(provider_event_data_control_local_[slot.value()].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, DoesNotMarkSlotAsInvalidOnMove)
{
    // Given an SampleAllocateePtr on an allocated slot
    auto slot = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    std::uint8_t data{};
    auto unit = SampleAllocateePtr(&data, control_composite_, consumer_event_data_control_local_, slot.value());

    // When moving it
    auto unit2 = std::move(unit);

    // Then the underlying slot is _not_ marked invalid
    EXPECT_FALSE(provider_event_data_control_local_[slot.value()].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, ReadySlotIsNotMarkedInvalidOnDestruction)
{
    // Given an SampleAllocateePtr on an allocated slot that is already marked as ready
    auto slot = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    provider_event_data_control_local_.EventReady(slot.value(), 0x42);
    std::uint8_t data{};
    {
        auto unit = SampleAllocateePtr(&data, control_composite_, consumer_event_data_control_local_, slot.value());
    }
    // When it goes out of scope

    // Then the underlying slot is _not_ marked invalid
    EXPECT_FALSE(provider_event_data_control_local_[slot.value()].IsInvalid());
    EXPECT_EQ(provider_event_data_control_local_[slot.value()].GetTimeStamp(), 0x42);
}

TEST_F(SampleAllocateePtrFixture, CanAccessUnderlyingSlot)
{
    RecordProperty("Verifies", "SCR-6367235");
    RecordProperty("Description", "A valid SampleAllocateePtr and SamplePtr shall reference a valid and correct slot.");
    RecordProperty("TestType", "Requirements-based test");
    RecordProperty("Priority", "1");
    RecordProperty("DerivationTechnique", "Analysis of requirements");

    // Given an SampleAllocateePtr on an allocated slot that is already marked as ready
    auto slot = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    provider_event_data_control_local_.EventReady(slot.value(), 0x42);
    std::uint8_t data{};
    auto unit = SampleAllocateePtr(&data, control_composite_, consumer_event_data_control_local_, slot.value());

    // When accessing which slot is associated with the SampleAllocateePtr
    auto referenced_slot = unit.GetReferencedSlot();

    // Then the underlying slot is the expected one and is valid
    EXPECT_EQ(referenced_slot, slot.value());
    EXPECT_FALSE(provider_event_data_control_local_[referenced_slot].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, ObeysOwnershipProperties)
{
    static_assert(std::is_move_constructible<SampleAllocateePtr>::value, "Is not move constructable");
    static_assert(std::is_move_assignable<SampleAllocateePtr>::value, "Is not move assignable");
    static_assert(!std::is_copy_constructible<SampleAllocateePtr>::value, "Is copy constructable");
    static_assert(!std::is_copy_assignable<SampleAllocateePtr>::value, "Is copy assignable");
}

TEST_F(SampleAllocateePtrFixture, MoveConstruct)
{
    // Given an SampleAllocateePtr on an allocated slot that is already marked as ready
    auto slot = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    provider_event_data_control_local_.EventReady(slot.value(), 0x42);
    std::uint8_t data{};
    auto unit = SampleAllocateePtr(&data, control_composite_, consumer_event_data_control_local_, slot.value());

    // When move constructing another SampleAllocateePtr from it
    SampleAllocateePtr unit2(std::move(unit));

    // Then the move constructed instance contains the original members
    EXPECT_EQ(unit2.GetReferencedSlot(), slot.value());
    EXPECT_TRUE(unit2);

    // ... and the underlying slot is still valid.
    EXPECT_FALSE(provider_event_data_control_local_[slot.value()].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, MoveAssign)
{
    // Given an SampleAllocateePtr on an allocated slot that is already marked as ready
    auto slot = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    provider_event_data_control_local_.EventReady(slot.value(), 0x42);
    std::uint8_t data{};
    auto unit = SampleAllocateePtr(&data, control_composite_, consumer_event_data_control_local_, slot.value());

    // When move assigning to another SampleAllocateePtr
    SampleAllocateePtr unit2 = std::move(unit);

    // Then the move constructed instance contains the original members
    EXPECT_EQ(unit2.GetReferencedSlot(), slot.value());
    EXPECT_TRUE(unit2);

    // ... and the underlying slot is still valid.
    EXPECT_FALSE(provider_event_data_control_local_[slot.value()].IsInvalid());
}

TEST_F(SampleAllocateePtrFixture, ConstructFromNullptr)
{
    // Given a SampleAllocateePtr constructed from nullptr
    auto unit = SampleAllocateePtr(nullptr);

    // expect that ...
    EXPECT_FALSE(unit);
    EXPECT_EQ(unit.GetReferencedSlot(), std::numeric_limits<SlotIndexType>::max());
    EXPECT_EQ(unit.get(), nullptr);
}

TEST_F(SampleAllocateePtrFixture, AssignNullptr)
{
    // Given an SampleAllocateePtr on an allocated slot
    auto slot = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    std::uint8_t data{};
    auto unit = SampleAllocateePtr(&data, control_composite_, consumer_event_data_control_local_, slot.value());

    // When assigning a nullptr to it
    unit = nullptr;

    // Then the underlying slot is marked invalid
    EXPECT_TRUE(provider_event_data_control_local_[slot.value()].IsInvalid());
    // and the SamplePtr doesn't hold a valid managed object.
    EXPECT_FALSE(unit);
}

TEST_F(SampleAllocateePtrFixture, GetReturnsTypeErasedPointerToUnderlyingData)
{
    // Given an SampleAllocateePtr on an allocated slot
    auto slot = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot.has_value());
    DummyStruct data{99, 42};
    auto unit = SampleAllocateePtr(&data, control_composite_, consumer_event_data_control_local_, slot.value());

    // When accessing the data via get() and casting the type-erased pointer back to the actual type
    auto* const typed_data = static_cast<DummyStruct*>(unit.get());

    // Then the values are as expected
    EXPECT_EQ(typed_data->member1_, 99);
    EXPECT_EQ(typed_data->member2_, 42);
}

TEST_F(SampleAllocateePtrFixture, SwapOp)
{
    // Given two SampleAllocateePtrs on allocated slots
    auto slot1 = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot1.has_value());
    DummyStruct data1{99, 42};
    auto unit1 = SampleAllocateePtr(&data1, control_composite_, consumer_event_data_control_local_, slot1.value());

    auto slot2 = provider_event_data_control_local_.AllocateNextSlot();
    ASSERT_TRUE(slot2.has_value());
    DummyStruct data2{10, 100};
    auto unit2 = SampleAllocateePtr(&data2, control_composite_, consumer_event_data_control_local_, slot2.value());

    // When swapping the SampleAllocateePtrs
    swap(unit1, unit2);

    // Then the underlying (type-erased) pointers are swapped
    EXPECT_EQ(unit1.get(), static_cast<void*>(&data2));
    EXPECT_EQ(unit2.get(), static_cast<void*>(&data1));

    // ... and the referenced slots are swapped as well
    EXPECT_EQ(unit1.GetReferencedSlot(), slot2.value());
    EXPECT_EQ(unit2.GetReferencedSlot(), slot1.value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
