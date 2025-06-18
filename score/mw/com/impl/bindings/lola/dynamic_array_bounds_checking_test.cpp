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
#include "score/containers/dynamic_array.h"

#include "score/memory/shared/fake/my_bounded_memory_resource.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <score/assert_support.hpp>
#include <score/utility.hpp>

#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace score::containers::test
{

template <typename ElementType, typename Allocator>
class DynamicArrayTestAttorney
{
  public:
    DynamicArrayTestAttorney(DynamicArray<ElementType, Allocator>& dynamic_array) : dynamic_array_{dynamic_array} {}

    void SetPointerToUnderlyingArray(void* const new_array_address)
    {
        dynamic_array_.dynamic_array_ = static_cast<ElementType*>(new_array_address);
    }

  private:
    DynamicArray<ElementType, Allocator>& dynamic_array_;
};

}  // namespace score::containers::test

namespace score::mw::com::impl::lola
{

using PointedType = std::uint64_t;
using TestDynamicArray =
    containers::DynamicArray<PointedType, memory::shared::PolymorphicOffsetPtrAllocator<PointedType>>;

class RegisteredMemoryPool
{
  public:
    // Memory pool of 400 bytes. Only the first 200 bytes are registered with the MemoryResourceRegistry
    using MemoryPoolType = std::array<std::uint8_t, 400>;
    static constexpr std::size_t kValidRangeInBytes{200U};

    alignas(std::max_align_t) MemoryPoolType memory_pool_{};
    MemoryPoolType::iterator start_of_valid_region_{memory_pool_.begin()};
    MemoryPoolType::iterator end_of_valid_region_{memory_pool_.begin() + kValidRangeInBytes};
};

template <typename DynamicArrayType>
class DynamicArrayBoundsCheckingFixture : public ::testing::Test
{
  public:
    static constexpr std::size_t kArraySize{4U};

    std::remove_const_t<DynamicArrayType>* CreateValidDynamicArrayInMemoryPool() noexcept
    {
        static_assert(sizeof(DynamicArrayType) <= RegisteredMemoryPool::kValidRangeInBytes);
        auto* const ptr_to_dynamic_array = memory_resource_.construct<std::remove_const_t<DynamicArrayType>>(
            kArraySize,
            memory::shared::PolymorphicOffsetPtrAllocator<PointedType>{memory_resource_.getMemoryResourceProxy()});

        std::uint64_t value{1U};
        for (auto& element : *ptr_to_dynamic_array)
        {
            element = value;
            ++value;
        }
        return ptr_to_dynamic_array;
    }

    DynamicArrayBoundsCheckingFixture& WithADynamicArrayWithinMemoryBounds() noexcept
    {
        ptr_to_dynamic_array_ = CreateValidDynamicArrayInMemoryPool();
        return *this;
    }

    DynamicArrayBoundsCheckingFixture& WithACorruptedDynamicArrayOverlappingMemoryBounds() noexcept
    {
        auto* const ptr_to_dynamic_array = CreateValidDynamicArrayInMemoryPool();

        const auto half_array_size_bytes = (kArraySize / 2U) * sizeof(PointedType);

        // Address should be corrupted such that all elements after the index (kArraySize / 2U) lie outside the memory
        // region.
        auto* const corrupted_array_overlapping_region = memory_pool_.end_of_valid_region_ - half_array_size_bytes;
        containers::test::DynamicArrayTestAttorney dynamic_array_attorney{*ptr_to_dynamic_array};
        dynamic_array_attorney.SetPointerToUnderlyingArray(corrupted_array_overlapping_region);

        ptr_to_dynamic_array_ = ptr_to_dynamic_array;
        return *this;
    }

    DynamicArrayBoundsCheckingFixture& WithACorruptedDynamicArrayOutsideMemoryBounds() noexcept
    {
        auto* const ptr_to_dynamic_array = CreateValidDynamicArrayInMemoryPool();

        // Address should be corrupted such that all elements lie outside the memory region.
        auto* const corrupted_array_overlapping_region = memory_pool_.end_of_valid_region_;
        containers::test::DynamicArrayTestAttorney dynamic_array_attorney{*ptr_to_dynamic_array};
        dynamic_array_attorney.SetPointerToUnderlyingArray(corrupted_array_overlapping_region);

        ptr_to_dynamic_array_ = ptr_to_dynamic_array;
        return *this;
    }

    std::size_t GetIndexForElementInsideMemoryBounds() const noexcept
    {
        return 1U;
    }

    std::size_t GetIndexForElementOutsideMemoryBounds() const noexcept
    {
        return (kArraySize / 2U) + 1U;
    }

    RegisteredMemoryPool memory_pool_{};
    memory::shared::test::MyBoundedMemoryResource memory_resource_{
        {memory_pool_.start_of_valid_region_, memory_pool_.end_of_valid_region_}};
    DynamicArrayType* ptr_to_dynamic_array_{nullptr};
};

// Gtest will run all tests in the DynamicArrayBoundsCheckingFixture once for every type, t, in MyTypes, such that
// TypeParam == t for each run. Note. Due to an apparent bug in gtest, the printed TypeParam when running the tests does
// not print whether the type is const (https://github.com/google/googletest/issues/2539). However, the actual types
// used in the tests do use the correct const / non-const types.
using MyTypes = ::testing::Types<TestDynamicArray, const TestDynamicArray>;
TYPED_TEST_SUITE(DynamicArrayBoundsCheckingFixture, MyTypes, );

template <typename T>
using DynamicArrayBoundsCheckingContractViolationFixture = DynamicArrayBoundsCheckingFixture<T>;
TYPED_TEST_SUITE(DynamicArrayBoundsCheckingContractViolationFixture, MyTypes, );

TYPED_TEST(DynamicArrayBoundsCheckingFixture, CallingAtForElementInMemoryRangeReturnsElement)
{
    // Given a DynamicArray that is fully within the bounds of a registered memory resource
    this->WithADynamicArrayWithinMemoryBounds();

    // When calling at() with an index that corresponds to an element within the registered memory region
    const auto& actual_element = this->ptr_to_dynamic_array_->at(this->GetIndexForElementInsideMemoryBounds());

    // Then a valid iterator should be returned
    const auto expected_pointed_type_value = this->GetIndexForElementInsideMemoryBounds() + 1;
    const PointedType expected_pointed_type(expected_pointed_type_value);
    EXPECT_EQ(actual_element, expected_pointed_type);
}

TYPED_TEST(DynamicArrayBoundsCheckingFixture, CallingIndexOperatorForElementInMemoryRangeReturnsElement)
{
    // Given a DynamicArray that is fully within the bounds of a registered memory resource
    this->WithADynamicArrayWithinMemoryBounds();

    // When calling operator[] with an index that corresponds to an element within the registered memory region
    const auto& actual_element = (*this->ptr_to_dynamic_array_)[this->GetIndexForElementInsideMemoryBounds()];

    // Then a valid iterator should be returned
    const auto expected_pointed_type_value = this->GetIndexForElementInsideMemoryBounds() + 1;
    const PointedType expected_pointed_type(expected_pointed_type_value);
    EXPECT_EQ(actual_element, expected_pointed_type);
}

TYPED_TEST(DynamicArrayBoundsCheckingFixture, CallingSizeReturnsSizeOfUnderlyingArray)
{
    // Given a DynamicArray that is fully within the bounds of a registered memory resource
    this->WithADynamicArrayWithinMemoryBounds();

    // When calling size()
    const auto actual_size = this->ptr_to_dynamic_array_->size();

    // Then the size of the array should be returned
    const auto expected_size = this->kArraySize;
    EXPECT_EQ(actual_size, expected_size);
}

TYPED_TEST(DynamicArrayBoundsCheckingFixture, CallingDataForElementInMemoryRangeReturnsPointer)
{
    // Given a DynamicArray that is fully within the bounds of a registered memory resource
    this->WithADynamicArrayWithinMemoryBounds();

    // When calling data()
    const auto* const actual_pointer = this->ptr_to_dynamic_array_->data();

    // Then a valid pointer should be returned
    const auto expected_pointed_type_value = 1;
    const PointedType expected_pointed_type(expected_pointed_type_value);
    ASSERT_NE(actual_pointer, nullptr);
    EXPECT_EQ(*actual_pointer, expected_pointed_type);
}

TYPED_TEST(DynamicArrayBoundsCheckingFixture, CallingBeginForElementInMemoryRangeReturnsIterator)
{
    // Given a DynamicArray that is fully within the bounds of a registered memory resource
    this->WithADynamicArrayWithinMemoryBounds();

    // When calling begin()
    const auto* const actual_iterator = this->ptr_to_dynamic_array_->begin();

    // Then a valid iterator should be returned
    const auto expected_pointed_type_value = 1;
    const PointedType expected_pointed_type(expected_pointed_type_value);
    ASSERT_NE(actual_iterator, nullptr);
    EXPECT_EQ(*actual_iterator, expected_pointed_type);
}

TYPED_TEST(DynamicArrayBoundsCheckingFixture, CallingCbeginForElementInMemoryRangeReturnsIterator)
{
    // Given a DynamicArray that is fully within the bounds of a registered memory resource
    this->WithADynamicArrayWithinMemoryBounds();

    // When calling cbegin()
    const auto* const actual_iterator = this->ptr_to_dynamic_array_->cbegin();

    // Then a valid iterator should be returned
    const auto expected_pointed_type_value = 1;
    const PointedType expected_pointed_type(expected_pointed_type_value);
    ASSERT_NE(actual_iterator, nullptr);
    EXPECT_EQ(*actual_iterator, expected_pointed_type);
}

TYPED_TEST(DynamicArrayBoundsCheckingFixture, CallingEndForElementInMemoryRangeReturnsIterator)
{
    // Given a DynamicArray that is fully within the bounds of a registered memory resource
    this->WithADynamicArrayWithinMemoryBounds();

    // When calling end()
    const auto* const actual_iterator = this->ptr_to_dynamic_array_->end();

    // Then a one-past-the-end-iterator should be returned
    const auto expected_pointed_type_value = this->kArraySize;
    const PointedType expected_pointed_type(expected_pointed_type_value);
    ASSERT_NE(actual_iterator, nullptr);
    const auto actual_last_element = actual_iterator - 1U;
    EXPECT_EQ(*actual_last_element, expected_pointed_type);
}

TYPED_TEST(DynamicArrayBoundsCheckingFixture, CallingCendForElementInMemoryRangeReturnsIterator)
{
    // Given a DynamicArray that is fully within the bounds of a registered memory resource
    this->WithADynamicArrayWithinMemoryBounds();

    // When calling cend()
    const auto* const actual_iterator = this->ptr_to_dynamic_array_->cend();

    // Then a one-past-the-end-iterator should be returned
    const auto expected_pointed_type_value = this->kArraySize;
    const PointedType expected_pointed_type(expected_pointed_type_value);
    ASSERT_NE(actual_iterator, nullptr);
    const auto actual_last_element = actual_iterator - 1U;
    EXPECT_EQ(*actual_last_element, expected_pointed_type);
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture, CallingAtForElementOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is partially within the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOverlappingMemoryBounds();

    // When calling at() with an index that corresponds to an element outside the registered memory region
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore =
                                     this->ptr_to_dynamic_array_->at(this->GetIndexForElementOutsideMemoryBounds()));
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture, CallingIndexOperatorForElementOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is partially within the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOverlappingMemoryBounds();

    // When calling at() with an index that corresponds to an element outside the registered memory region
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore =
                                     (*this->ptr_to_dynamic_array_)[this->GetIndexForElementOutsideMemoryBounds()]);
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingDataForArrayWithStartAndEndAddressesOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is fully outside the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOutsideMemoryBounds();

    // When calling data()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->data());
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingDataForArrayWithStartAddressInAndEndAddressOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is partially within the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOverlappingMemoryBounds();

    // When calling data()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->data());
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingBeginForArrayWithStartAndEndAddressesOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is fully outside the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOutsideMemoryBounds();

    // When calling begin()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->begin());
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingBeginForArrayWithStartAddressInAndEndAddressOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is partially within the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOverlappingMemoryBounds();

    // When calling begin()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->begin());
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingCbeginForArrayWithStartAndEndAddressesOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is fully outside the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOutsideMemoryBounds();

    // When calling cbegin()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->cbegin());
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingCbeginForArrayWithStartAddressInAndEndAddressOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is partially within the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOverlappingMemoryBounds();

    // When calling cbegin()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->cbegin());
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingEndForArrayWithStartAndEndAddressesOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is fully outside the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOutsideMemoryBounds();

    // When calling end()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->end());
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingEndForArrayWithStartAddressInAndEndAddressOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is partially within the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOverlappingMemoryBounds();

    // When calling end()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->end());
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingCendForArrayWithStartAndEndAddressesOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is fully outside the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOutsideMemoryBounds();

    // When calling cend()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->cend());
}

TYPED_TEST(DynamicArrayBoundsCheckingContractViolationFixture,
           CallingCendForArrayWithStartAddressInAndEndAddressOutOfMemoryRangeTerminates)
{
    // Given a DynamicArray that is partially within the bounds of a registered memory resource
    this->WithACorruptedDynamicArrayOverlappingMemoryBounds();

    // When calling cend()
    // Then the program should terminate
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = this->ptr_to_dynamic_array_->cend());
}

}  // namespace score::mw::com::impl::lola
