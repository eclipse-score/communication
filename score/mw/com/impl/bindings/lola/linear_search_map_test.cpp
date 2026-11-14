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
#include "score/mw/com/impl/bindings/lola/linear_search_map.h"

#include "score/memory/shared/new_delete_delegate_resource.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <scoped_allocator>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

const std::uint64_t kMemoryResourceId{42U};
constexpr std::int32_t kSomeKey{1};
constexpr std::int32_t kSomeValue{100};

using TestMap = LinearSearchMap<std::int32_t, std::int32_t>;

class LinearSearchMapFixture : public ::testing::Test
{
  protected:
    memory::shared::NewDeleteDelegateMemoryResource memory_{kMemoryResourceId};
};

TEST_F(LinearSearchMapFixture, IsEmptyAfterConstruction)
{
    // When constructing a LinearSearchMap with a capacity of 4
    TestMap unit{4U};

    // Then it is empty, has size 0 and reports the requested capacity
    EXPECT_TRUE(unit.empty());
    EXPECT_EQ(unit.size(), 0U);
    EXPECT_EQ(unit.capacity(), 4U);
}

TEST_F(LinearSearchMapFixture, BeginAndEndEqualAfterConstruction)
{
    // When constructing a LinearSearchMap with a capacity of 4
    TestMap unit{4U};

    // Then begin and end iterators are equal (no elements to iterate)
    EXPECT_EQ(unit.begin(), unit.end());
}

TEST_F(LinearSearchMapFixture, EmplaceSucceedsAndReturnsCorrectIterator)
{
    // Given an empty LinearSearchMap
    TestMap unit{4U};

    // When emplacing a new key/value pair
    const auto result = unit.emplace(kSomeKey, kSomeValue);

    // Then the insertion succeeds and the element is retrievable via the returned iterator
    EXPECT_TRUE(result.second);
    ASSERT_NE(result.first, unit.end());
    EXPECT_EQ(result.first->first, kSomeKey);
    EXPECT_EQ(result.first->second, kSomeValue);
    EXPECT_EQ(unit.size(), 1U);
    EXPECT_FALSE(unit.empty());
}

TEST_F(LinearSearchMapFixture, EmplaceWithPiecewiseConstructSucceedsAndReturnsCorrectIterator)
{
    // Given an empty LinearSearchMap
    TestMap unit{4U};

    // When emplacing a new element using the piecewise-construct overload
    const auto result =
        unit.emplace(std::piecewise_construct, std::forward_as_tuple(kSomeKey), std::forward_as_tuple(kSomeValue));

    // Then the insertion succeeds and the element is retrievable via the returned iterator
    EXPECT_TRUE(result.second);
    ASSERT_NE(result.first, unit.end());
    EXPECT_EQ(result.first->first, kSomeKey);
    EXPECT_EQ(result.first->second, kSomeValue);
}

TEST_F(LinearSearchMapFixture, EmplaceWithDuplicateKeyDoesNotInsert)
{
    // Given a LinearSearchMap that already contains the key 1
    TestMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);

    // When emplacing another element with the same key and different value
    const auto result = unit.emplace(kSomeKey, kSomeValue + 5);

    // Then no insertion takes place and the originally inserted value is preserved
    EXPECT_FALSE(result.second);
    EXPECT_EQ(unit.size(), 1U);
    EXPECT_EQ(result.first->second, kSomeValue);
}

TEST_F(LinearSearchMapFixture, FindReturnsInsertedElement)
{
    // Given a LinearSearchMap containing several elements
    TestMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);
    std::ignore = unit.emplace(kSomeKey + 1, kSomeValue + 1);
    std::ignore = unit.emplace(kSomeKey + 2, kSomeValue + 2);

    // When searching for an existing key
    const auto it = unit.find(kSomeKey);

    // Then the corresponding element is returned
    ASSERT_NE(it, unit.end());
    EXPECT_EQ(it->second, kSomeValue);
}

TEST_F(LinearSearchMapFixture, FindReturnsEndForMissingKey)
{
    // Given a LinearSearchMap containing a single element
    TestMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);

    // When searching for a key that is not present
    // Then the end iterator is returned
    EXPECT_EQ(unit.find(99), unit.end());
}

TEST_F(LinearSearchMapFixture, ConstFindReturnsInsertedElement)
{
    // Given a const reference to a LinearSearchMap containing one element
    TestMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);
    const TestMap& const_unit = unit;

    // When searching via the const overload of find
    const auto it = const_unit.find(kSomeKey);

    // Then an existing key is found and the value matches.
    ASSERT_NE(it, const_unit.cend());
    EXPECT_EQ(it->second, kSomeValue);
}

TEST_F(LinearSearchMapFixture, ConstFindReturnsEndForMissingKey)
{
    // Given a const reference to a LinearSearchMap containing one element
    TestMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);
    const TestMap& const_unit = unit;

    // When searching via the const overload of find for a non existent key
    const auto it = const_unit.find(kSomeKey + 5);

    // Then it yields the const end iterator
    EXPECT_EQ(it, const_unit.cend());
}

TEST_F(LinearSearchMapFixture, AtReturnsMappedValue)
{
    // Given a LinearSearchMap containing one element
    TestMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);

    // When accessing the mapped value via at
    // Then the stored value is returned
    EXPECT_EQ(unit.at(kSomeKey), kSomeValue);

    // and can be modified through the returned reference
    unit.at(kSomeKey) = kSomeValue + 1;
    EXPECT_EQ(unit.at(kSomeKey), kSomeValue + 1);
}

TEST_F(LinearSearchMapFixture, IterationVisitsAllElements)
{
    // Given a LinearSearchMap containing two elements
    TestMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);
    std::ignore = unit.emplace(kSomeKey + 1, kSomeValue + 1);

    // When iterating over all elements and accumulating keys and values
    std::int32_t sum_keys{0};
    std::int32_t sum_values{0};
    for (const auto& element : unit)
    {
        sum_keys += element.first;
        sum_values += element.second;
    }

    // Then every element is visited exactly once
    EXPECT_EQ(sum_keys, kSomeKey + (kSomeKey + 1));
    EXPECT_EQ(sum_values, kSomeValue + (kSomeValue + 1));
}

// A custom key-equality predicate that considers two keys equal if they have the same absolute value.
struct AbsoluteValueEqual
{
    bool operator()(const std::int32_t lhs, const std::int32_t rhs) const noexcept
    {
        return (lhs < 0 ? -lhs : lhs) == (rhs < 0 ? -rhs : rhs);
    }
};

using CustomEqualMap = LinearSearchMap<std::int32_t, std::int32_t, AbsoluteValueEqual>;

TEST_F(LinearSearchMapFixture, CustomKeyEqualIsUsedForFind)
{
    // Given a LinearSearchMap using a custom absolute-value key-equality predicate, containing the key kSomeKey
    CustomEqualMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);

    // When searching for -kSomeKey, which the custom predicate considers equal to kSomeKey
    const auto it = unit.find(-kSomeKey);

    // Then the element stored under key kSomeKey is found
    ASSERT_NE(it, unit.end());
    EXPECT_EQ(it->second, kSomeValue);
}

TEST_F(LinearSearchMapFixture, CustomKeyEqualPreventsDuplicateInsertion)
{
    // Given a LinearSearchMap using a custom absolute-value key-equality predicate, containing the key kSomeKey
    CustomEqualMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);

    // When emplacing -kSomeKey, which the custom predicate treats as a duplicate of kSomeKey
    const auto result = unit.emplace(-kSomeKey, kSomeValue + 1);

    // Then no insertion takes place and the original value is preserved
    EXPECT_FALSE(result.second);
    EXPECT_EQ(unit.size(), 1U);
    EXPECT_EQ(result.first->second, kSomeValue);
}

TEST_F(LinearSearchMapFixture, KeyEqAccessorReturnsPredicate)
{
    // Given a LinearSearchMap using a custom absolute-value key-equality predicate
    CustomEqualMap unit{4U};

    // When retrieving the predicate via key_eq
    const auto predicate = unit.key_eq();

    // Then the returned predicate exhibits the custom equality semantics
    EXPECT_TRUE(predicate(-7, 7));
    EXPECT_FALSE(predicate(-7, 8));
}

/// \brief allocator-aware type used to directly observe which allocator instance was passed down by
/// the std::scoped_allocator_adaptor machinery, independent of the intricacies of a "real" container such as
/// std::vector.
class AllocatorAwareTestType
{
  public:
    using allocator_type = memory::shared::PolymorphicOffsetPtrAllocator<AllocatorAwareTestType>;

    // Trailing-allocator constructor, matching the standard "uses-allocator construction" convention expected by
    // std::scoped_allocator_adaptor.
    explicit AllocatorAwareTestType(const std::int32_t value, const allocator_type& allocator = allocator_type{})
        : value_{value}, allocator_{allocator}
    {
    }

    // Allocator-extended copy constructor, required so std::scoped_allocator_adaptor can forward the allocator when
    // copying an already constructed probe (e.g. via LinearSearchMap::emplace(const Key&, MappedArg&&)).
    AllocatorAwareTestType(const AllocatorAwareTestType& other, const allocator_type& allocator)
        : value_{other.value_}, allocator_{allocator}
    {
    }

    std::int32_t value() const noexcept
    {
        return value_;
    }

    const allocator_type& get_allocator() const noexcept
    {
        return allocator_;
    }

    // intentionally ignores the allocator, matching the usual semantics of allocator-aware types
    friend bool operator==(const AllocatorAwareTestType& lhs, const AllocatorAwareTestType& rhs) noexcept
    {
        return lhs.value_ == rhs.value_;
    }

  private:
    std::int32_t value_;
    allocator_type allocator_;
};

/// \brief Alias for a LinearSearchMap that explicitly opts into allocator forwarding to allocator-aware Key/
/// MappedType instances, by using a std::scoped_allocator_adaptor wrapping a PolymorphicOffsetPtrAllocator as its
/// Allocator template argument. LinearSearchMap's default Allocator (plain std::allocator) does *not* perform any
/// such forwarding (see std::scoped_allocator_adaptor's specialized std::pair-aware construct() overloads)
template <typename Key, typename MappedType, typename KeyEqual = std::equal_to<Key>>
using ScopedAllocatorAwareMap = LinearSearchMap<
    Key,
    MappedType,
    KeyEqual,
    std::scoped_allocator_adaptor<memory::shared::PolymorphicOffsetPtrAllocator<std::pair<Key, MappedType>>>>;

using MapWithAllocatorAwareMappedType = ScopedAllocatorAwareMap<std::int32_t, AllocatorAwareTestType>;

TEST_F(LinearSearchMapFixture, AllocatorIsForwardedToAllocatorAwareMappedTypeViaEmplace)
{
    // Given an empty LinearSearchMap whose MappedType is allocator-aware, constructed with an explicit
    // std::scoped_allocator_adaptor-wrapped allocator bound to memory_
    MapWithAllocatorAwareMappedType unit{4U, {}, typename MapWithAllocatorAwareMappedType::allocator_type{memory_}};

    // When emplacing a new element via the convenience (non-piecewise) emplace overload
    const auto result = unit.emplace(kSomeKey, kSomeValue);

    // Then the insertion succeeds and the stored MappedType instance was constructed with the same allocator
    // (i.e. pointing at the same underlying memory resource) as the one the LinearSearchMap itself was constructed
    // with.
    ASSERT_TRUE(result.second);
    EXPECT_EQ(result.first->second.value(), kSomeValue);
    const memory::shared::PolymorphicOffsetPtrAllocator<AllocatorAwareTestType> expected_allocator{memory_};
    EXPECT_EQ(result.first->second.get_allocator(), expected_allocator);
}

TEST_F(LinearSearchMapFixture, AllocatorIsForwardedToAllocatorAwareMappedTypeViaPiecewiseEmplace)
{
    // Given an empty LinearSearchMap whose MappedType is allocator-aware, constructed with an explicit
    // std::scoped_allocator_adaptor-wrapped allocator bound to memory_
    MapWithAllocatorAwareMappedType unit{4U, {}, typename MapWithAllocatorAwareMappedType::allocator_type{memory_}};

    // When emplacing a new element via the piecewise-construct overload
    const auto result =
        unit.emplace(std::piecewise_construct, std::forward_as_tuple(kSomeKey), std::forward_as_tuple(kSomeValue));

    // Then the stored MappedType instance was constructed with the LinearSearchMap's allocator
    ASSERT_TRUE(result.second);
    EXPECT_EQ(result.first->second.value(), kSomeValue);
    const memory::shared::PolymorphicOffsetPtrAllocator<AllocatorAwareTestType> expected_allocator{memory_};
    EXPECT_EQ(result.first->second.get_allocator(), expected_allocator);
}

TEST_F(LinearSearchMapFixture, AllocatorIsForwardedToAllocatorAwareKeyTypeViaPiecewiseEmplace)
{
    using MapWithAllocatorAwareKeyType = ScopedAllocatorAwareMap<AllocatorAwareTestType, std::int32_t>;
    // Given an empty LinearSearchMap whose Key is allocator-aware, constructed with an explicit
    // std::scoped_allocator_adaptor-wrapped allocator bound to memory_
    MapWithAllocatorAwareKeyType unit{4U, {}, typename MapWithAllocatorAwareKeyType::allocator_type{memory_}};

    // When emplacing a new element via the piecewise-construct overload
    const auto result =
        unit.emplace(std::piecewise_construct, std::forward_as_tuple(kSomeKey), std::forward_as_tuple(kSomeValue));

    // Then the stored Key instance was constructed with the LinearSearchMap's allocator as well (the forwarding
    // mechanism of std::scoped_allocator_adaptor applies symmetrically to both pair members).
    ASSERT_TRUE(result.second);
    EXPECT_EQ(result.first->first.value(), kSomeKey);
    const memory::shared::PolymorphicOffsetPtrAllocator<AllocatorAwareTestType> expected_allocator{memory_};
    EXPECT_EQ(result.first->first.get_allocator(), expected_allocator);

    // And find() still works, since AllocatorAwareTestType's operator== ignores the allocator.
    EXPECT_NE(unit.find(AllocatorAwareTestType{kSomeKey}), unit.end());
}

TEST_F(LinearSearchMapFixture, AllocatorIsForwardedIntoAllocatorAwareVectorMappedType)
{
    using MapWithVectorMappedType =
        ScopedAllocatorAwareMap<std::int32_t,
                                std::vector<std::int32_t, memory::shared::PolymorphicOffsetPtrAllocator<std::int32_t>>>;
    // Given an empty LinearSearchMap whose MappedType is a std::vector that itself uses
    // memory::shared::PolymorphicOffsetPtrAllocator<int> (i.e. it declares the shared-memory allocator as its own
    // allocator_type), constructed with an explicit std::scoped_allocator_adaptor-wrapped allocator bound to memory_
    MapWithVectorMappedType unit{4U, {}, typename MapWithVectorMappedType::allocator_type{memory_}};

    // When emplacing a new, initially empty vector as mapped value
    const auto result =
        unit.emplace(std::piecewise_construct, std::forward_as_tuple(kSomeKey), std::forward_as_tuple());

    // Then the vector was constructed with the same (shared-memory-resource-bound) allocator as the LinearSearchMap
    ASSERT_TRUE(result.second);
    const memory::shared::PolymorphicOffsetPtrAllocator<std::int32_t> expected_allocator{memory_};
    EXPECT_EQ(result.first->second.get_allocator(), expected_allocator);

    // and the vector remains fully usable (its elements are allocated via the forwarded, shared-memory capable
    // allocator).
    result.first->second.push_back(kSomeValue);
    result.first->second.push_back(kSomeValue + 1);
    ASSERT_EQ(result.first->second.size(), 2U);
    EXPECT_EQ(result.first->second.at(0), kSomeValue);
    EXPECT_EQ(result.first->second.at(1), kSomeValue + 1);
}

using LinearSearchMapDeathTest = LinearSearchMapFixture;
TEST_F(LinearSearchMapDeathTest, AddMoreElementsThanCapacity)
{
    // Given a LinearSearchMap which contains capacity count elements
    TestMap unit{4U};
    std::ignore = unit.emplace(kSomeKey, kSomeValue);
    std::ignore = unit.emplace(kSomeKey + 1, kSomeValue + 1);
    std::ignore = unit.emplace(kSomeKey + 2, kSomeValue + 2);
    std::ignore = unit.emplace(kSomeKey + 3, kSomeValue + 3);

    // When adding another element
    // Then the program terminates
    EXPECT_DEATH(unit.emplace(kSomeKey + 4, kSomeValue + 4), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::lola
