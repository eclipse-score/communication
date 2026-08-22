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
#include "score/mw/com/impl/bindings/lola/event_data_storage.h"

#include "score/memory/data_type_size_info.h"
#include "score/memory/shared/new_delete_delegate_resource.h"

#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint64_t kMemoryResourceId{42U};
constexpr SlotIndexType kNumberOfSlots{4U};

/// \brief A trivial dummy type that requires std::max_align_t alignment.
/// \details Used (in addition to plain integral types) as one of the sample types EventDataStorage is typed-tested
/// with, to make sure EventDataStorage also correctly handles the "worst case" alignment requirement.
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
/// TypeParam's actual memory::DataTypeSizeInfo.
template <typename T>
class EventDataStorageTypedTest : public ::testing::Test
{
  protected:
    memory::shared::NewDeleteDelegateMemoryResource memory_resource_{kMemoryResourceId};
    memory::DataTypeSizeInfo sample_size_info_{sizeof(T), alignof(T)};
    EventDataStorage unit_{memory_resource_, kNumberOfSlots, sample_size_info_};
};

using SampleTypes = ::testing::Types<std::uint16_t, std::uint64_t, MaxAlignedDummyStruct>;
TYPED_TEST_SUITE(EventDataStorageTypedTest, SampleTypes, );

TYPED_TEST(EventDataStorageTypedTest, GetTypeErasedDataSlotReturnsCorrectlyAlignedAndWritableSlotForEveryIndex)
{
    // Given an EventDataStorage constructed for TypeParam (see fixture)

    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        // When retrieving the type-erased pointer to the data slot at slot_index
        void* const type_erased_slot = this->unit_.GetTypeErasedDataSlot(slot_index, sizeof(TypeParam));

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

TYPED_TEST(EventDataStorageTypedTest, DataSlotsOfDifferentIndicesDoNotOverlap)
{
    // Given an EventDataStorage constructed for TypeParam (see fixture)

    // When writing a distinct typed-value into every one of its data slots
    std::vector<TypeParam> written_values{};
    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        const TypeParam value = MakeValue<TypeParam>(static_cast<std::uint8_t>(slot_index + 1U));
        written_values.push_back(value);

        auto* const typed_slot =
            static_cast<TypeParam*>(this->unit_.GetTypeErasedDataSlot(slot_index, sizeof(TypeParam)));
        *typed_slot = value;
    }

    // Then every data slot still contains the very same distinct value that was written to it, i.e. writing to one
    // slot did not corrupt/overlap the contents of another slot.
    for (SlotIndexType slot_index = 0U; slot_index < kNumberOfSlots; ++slot_index)
    {
        auto* const typed_slot =
            static_cast<TypeParam*>(this->unit_.GetTypeErasedDataSlot(slot_index, sizeof(TypeParam)));
        EXPECT_TRUE(BytesEqual(*typed_slot, written_values[slot_index]));
    }
}

TEST(EventDataStorageDeathTest, GetTypeErasedDataSlotTerminatesOnDataSizeMismatch)
{
    // Given an EventDataStorage constructed for a std::uint32_t sample type
    memory::shared::NewDeleteDelegateMemoryResource memory_resource{kMemoryResourceId};
    const memory::DataTypeSizeInfo sample_size_info{sizeof(std::uint32_t), alignof(std::uint32_t)};
    EventDataStorage unit{memory_resource, kNumberOfSlots, sample_size_info};

    // When requesting a data slot with a data_size that does not match the sample type's actual size
    // Then the program terminates, since the caller's size expectation doesn't match the storage's sample size.
    EXPECT_DEATH(unit.GetTypeErasedDataSlot(0U, sizeof(std::uint32_t) + 1U), ".*");
}

TEST(EventDataStorageDeathTest, GetTypeErasedDataSlotTerminatesOnOutOfBoundsIndex)
{
    // Given an EventDataStorage constructed for a std::uint32_t sample type with kNumberOfSlots slots
    memory::shared::NewDeleteDelegateMemoryResource memory_resource{kMemoryResourceId};
    const memory::DataTypeSizeInfo sample_size_info{sizeof(std::uint32_t), alignof(std::uint32_t)};
    EventDataStorage unit{memory_resource, kNumberOfSlots, sample_size_info};

    // When requesting a data slot with an index that is out of bounds
    // Then the program terminates.
    EXPECT_DEATH(unit.GetTypeErasedDataSlot(kNumberOfSlots, sizeof(std::uint32_t)), ".*");
}

TEST(EventDataStorageDeathTest, ConstructionTerminatesOnRawSlotArraySizeOverflow)
{
    // Given a number_of_slots and a per-sample memory::DataTypeSizeInfo whose sizes, when multiplied to calculate
    // the total size of the raw event-data slot-array, overflow std::size_t
    memory::shared::NewDeleteDelegateMemoryResource memory_resource{kMemoryResourceId};
    constexpr std::size_t alignment{alignof(std::max_align_t)};
    const memory::DataTypeSizeInfo overflowing_sample_size_info{
        (std::numeric_limits<std::size_t>::max() / alignment) * alignment, alignment};
    constexpr SlotIndexType number_of_slots{2U};

    // When constructing an EventDataStorage from this sizing information
    // Then the program terminates, since calculating the total raw slot-array size would silently overflow.
    EXPECT_DEATH(
        score::cpp::ignore = (EventDataStorage{memory_resource, number_of_slots, overflowing_sample_size_info}), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
