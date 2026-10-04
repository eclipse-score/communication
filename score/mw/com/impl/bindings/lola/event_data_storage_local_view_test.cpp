/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/lola/event_data_storage_local_view.h"

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/new_delete_delegate_resource.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint64_t kMemoryResourceId{42U};
constexpr SlotIndexType kNumberOfSlots{4U};

/// \brief A trivial dummy type that requires std::max_align_t alignment.
/// \details Used (in addition to plain integral types) as one of the sample types EventDataStorageLocalView is
/// typed-tested with, to make sure it also correctly handles the "worst case" alignment requirement.
struct MaxAlignedDummyStruct
{
    std::byte byte_member;
    std::max_align_t max_align_member;
};

/// \brief Fills every byte of value with pattern, so that two values created with a differing pattern are guaranteed
/// to compare unequal (see BytesEqual()), regardless of TypeParam's actual member layout.
template <typename T>
T MakeValue(const std::uint8_t pattern)
{
    T value{};
    std::memset(&value, pattern, sizeof(T));
    return value;
}

/// \brief Compares lhs and rhs byte-by-byte.
/// \details We cannot rely on operator== being defined for every TypeParam (e.g. MaxAlignedDummyStruct doesn't define
/// one), so we compare the raw bytes instead.
template <typename T>
bool BytesEqual(const T& lhs, const T& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(T)) == 0;
}

/// \brief Templated test fixture that constructs a real EventDataStorage for TypeParam, sized/aligned according to
/// TypeParam's actual memory::DataTypeSizeInfo, and an EventDataStorageLocalView on top of it (which is the unit
/// under test).
template <typename T>
class EventDataStorageLocalViewTypedTest : public ::testing::Test
{
  protected:
    memory::shared::NewDeleteDelegateMemoryResource memory_resource_{kMemoryResourceId};
    memory::DataTypeSizeInfo sample_size_info_{sizeof(T), alignof(T)};
    EventDataStorage event_data_storage_{memory_resource_, kNumberOfSlots, sample_size_info_};
    EventDataStorageLocalView unit_{event_data_storage_, sample_size_info_};
};

using SampleTypes = ::testing::Types<std::uint16_t, std::uint64_t, MaxAlignedDummyStruct>;
TYPED_TEST_SUITE(EventDataStorageLocalViewTypedTest, SampleTypes, );

TYPED_TEST(EventDataStorageLocalViewTypedTest, GetTypeErasedDataSlotReturnsCorrectlyAlignedAndWritableSlotForEveryIndex)
{
    // Given an EventDataStorageLocalView constructed on top of an EventDataStorage for TypeParam (see fixture)

    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        // When retrieving the type-erased pointer to the data slot at slot_index
        void* const type_erased_slot = this->unit_.GetTypeErasedDataSlot(slot_index);

        // Then the returned pointer is non-null and correctly aligned for TypeParam
        ASSERT_NE(type_erased_slot, nullptr);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(type_erased_slot) % alignof(TypeParam), 0U);

        // and casting it to a TypeParam* and writing/reading a value through it works without crashing (e.g. due to
        // an alignment violation) and yields back the very same value that was written.
        auto* const typed_slot = static_cast<TypeParam*>(type_erased_slot);
        const TypeParam value_to_write = MakeValue<TypeParam>(static_cast<std::uint8_t>(slot_index + 1U));
        *typed_slot = value_to_write;

        EXPECT_TRUE(BytesEqual(*typed_slot, value_to_write));
    }
}

TYPED_TEST(EventDataStorageLocalViewTypedTest, GetTypeErasedDataSlotReturnsSameAddressAsUnderlyingEventDataStorage)
{
    // Given an EventDataStorageLocalView constructed on top of an EventDataStorage for TypeParam (see fixture)

    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        // When retrieving the type-erased pointer to the data slot at slot_index via the local view
        void* const type_erased_slot_via_view = this->unit_.GetTypeErasedDataSlot(slot_index);

        // Then it points at the very same address as the corresponding offset into the underlying EventDataStorage's
        // raw byte range, i.e. the local view is a pointer-equivalent view onto the shared-memory storage.
        auto* const expected_address =
            this->event_data_storage_.GetTypeErasedDataSlotsStart() + (slot_index * sizeof(TypeParam));
        EXPECT_EQ(type_erased_slot_via_view, static_cast<void*>(expected_address));
    }
}

TYPED_TEST(EventDataStorageLocalViewTypedTest, DataSlotsOfDifferentIndicesDoNotOverlap)
{
    // Given an EventDataStorageLocalView constructed on top of an EventDataStorage for TypeParam (see fixture)

    // When writing a distinct typed-value into every one of its data slots
    std::vector<TypeParam> written_values{};
    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        const TypeParam value = MakeValue<TypeParam>(static_cast<std::uint8_t>(slot_index + 1U));
        written_values.push_back(value);

        auto* const typed_slot = static_cast<TypeParam*>(this->unit_.GetTypeErasedDataSlot(slot_index));
        *typed_slot = value;
    }

    // Then every data slot still contains the very same distinct value that was written to it, i.e. writing to one
    // slot did not corrupt/overlap the contents of another slot.
    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        auto* const typed_slot = static_cast<TypeParam*>(this->unit_.GetTypeErasedDataSlot(slot_index));
        EXPECT_TRUE(BytesEqual(*typed_slot, written_values[slot_index]));
    }
}

TYPED_TEST(EventDataStorageLocalViewTypedTest, GetTypeErasedDataSlotReturnsDistinctPointersForEveryIndex)
{
    // Given an EventDataStorageLocalView constructed on top of an EventDataStorage for TypeParam (see fixture)

    // When retrieving the type-erased pointer for every slot index
    std::set<void*> unique_slot_pointers{};
    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        unique_slot_pointers.insert(this->unit_.GetTypeErasedDataSlot(slot_index));
    }

    // Then all returned pointers are distinct
    EXPECT_EQ(unique_slot_pointers.size(), kNumberOfSlots);
}

TEST(EventDataStorageLocalViewMoveTest, MoveConstructedViewReturnsSameSlotAddressesAsOriginal)
{
    // Given an EventDataStorageLocalView constructed on top of an EventDataStorage
    memory::shared::NewDeleteDelegateMemoryResource memory_resource{kMemoryResourceId};
    const memory::DataTypeSizeInfo sample_size_info{sizeof(std::uint32_t), alignof(std::uint32_t)};
    EventDataStorage event_data_storage{memory_resource, kNumberOfSlots, sample_size_info};
    EventDataStorageLocalView unit{event_data_storage, sample_size_info};

    void* const slot_address_before_move = unit.GetTypeErasedDataSlot(0U);

    // When move-constructing a new view from it
    EventDataStorageLocalView moved_unit{std::move(unit)};

    // Then the moved-to view returns the very same slot addresses as the original view did before the move
    EXPECT_EQ(moved_unit.GetTypeErasedDataSlot(0U), slot_address_before_move);
}

TEST(EventDataStorageLocalViewDeathTest, GetTypeErasedDataSlotTerminatesOnOutOfBoundsIndex)
{
    // Given an EventDataStorageLocalView constructed on top of an EventDataStorage with kNumberOfSlots slots
    memory::shared::NewDeleteDelegateMemoryResource memory_resource{kMemoryResourceId};
    const memory::DataTypeSizeInfo sample_size_info{sizeof(std::uint32_t), alignof(std::uint32_t)};
    EventDataStorage event_data_storage{memory_resource, kNumberOfSlots, sample_size_info};
    EventDataStorageLocalView unit{event_data_storage, sample_size_info};

    // When requesting a data slot with an index that is out of bounds
    // Then the program terminates.
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit.GetTypeErasedDataSlot(kNumberOfSlots));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
