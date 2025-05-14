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
#ifndef SCORE_LIB_MEMORY_SHARED_TEST_OFFSET_PTR_OFFSET_PTR_TEST_RESOURCES_H
#define SCORE_LIB_MEMORY_SHARED_TEST_OFFSET_PTR_OFFSET_PTR_TEST_RESOURCES_H

#include "score/memory/shared/fake/my_bounded_memory_resource.h"
#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/offset_ptr.h"

#include <score/assert.hpp>

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::memory::shared::test
{

constexpr std::size_t kDefaultMemoryRegionSize{1000U};

using UseRegisteredMemoryResource = std::bool_constant<true>;
using UseUnregisteredMemoryResource = std::bool_constant<false>;

struct ComplexTypeStruct
{
    int a;
    std::uint8_t b;
    std::vector<int> c;
    std::string d;
    std::array<std::uint8_t, 10> e;
};
inline bool operator==(const ComplexTypeStruct& lhs, const ComplexTypeStruct& rhs) noexcept
{
    return lhs.a == rhs.a;
}

/// \brief Helper struct to contain a void type since we cannot use void directly in a std::pair
struct VoidType
{
    using Type = void;
};
struct IntType
{
    using Type = int;

    static Type CreateDummyValue()
    {
        return 10;
    }
};
struct UInt8Type
{
    using Type = std::uint8_t;

    static Type CreateDummyValue()
    {
        return 11U;
    }
};
struct VeryLargeType
{
    using Type = std::array<std::uint8_t, 100>;

    static Type CreateDummyValue()
    {
        Type dummy_type{};
        for (std::uint8_t i = 0; i < dummy_type.size(); ++i)
        {
            dummy_type[i] = i;
        }
        return dummy_type;
    }
};
struct ComplexType
{
    using Type = ComplexTypeStruct;

    static Type CreateDummyValue()
    {
        Type dummy_type{};
        dummy_type.a = 10;
        return dummy_type;
    }
};
struct ConstVoidType
{
    using Type = const VoidType::Type;
};
struct ConstIntType
{
    using Type = const IntType::Type;

    static IntType::Type CreateDummyValue()
    {
        return IntType::CreateDummyValue();
    }
};
struct ConstUInt8Type
{
    using Type = const UInt8Type::Type;

    static UInt8Type::Type CreateDummyValue()
    {
        return UInt8Type::CreateDummyValue();
    }
};
struct ConstVeryLargeType
{
    using Type = const VeryLargeType::Type;

    static VeryLargeType::Type CreateDummyValue()
    {
        return VeryLargeType::CreateDummyValue();
    }
};
struct ConstComplexType
{
    using Type = const ComplexType::Type;

    static ComplexType::Type CreateDummyValue()
    {
        return ComplexType::CreateDummyValue();
    }
};

/// \brief Helper class for creating OffsetPtrs and the pointed to objects we use a class to avoid partial function
///        template specialisations
template <typename PointedType>
class OffsetPtrCreator
{
  public:
    /// \brief Create an OffsetPtr and return the OffsetPtr and pointed-to address.
    ///
    /// Since the functionality of the OffsetPtr is dependent on the memory region in which it resides, we return a
    /// reference to the OffsetPtr so that it is not copied to the stack.
    template <typename... Args>
    static std::pair<std::reference_wrapper<OffsetPtr<PointedType>>, PointedType*> CreateOffsetPtrInResource(
        ManagedMemoryResource& memory_resource,
        Args&&... args)
    {
        auto* const value_ptr = CreatePointedToObject(memory_resource, std::forward<Args>(args)...);
        auto* const ptr_to_offset_ptr = memory_resource.construct<OffsetPtr<PointedType>>(value_ptr);

        SCORE_LANGUAGE_FUTURECPP_ASSERT(ptr_to_offset_ptr != nullptr);
        return {*ptr_to_offset_ptr, value_ptr};
    }

    template <typename... Args>
    static PointedType* CreatePointedToObject(ManagedMemoryResource& memory_resource, Args&&... args)
    {
        return memory_resource.construct<PointedType>(std::forward<Args>(args)...);
    }

    static PointedType* GetRawPointer(const OffsetPtr<PointedType>& offset_ptr)
    {
        return offset_ptr.get();
    }
};

template <>
class OffsetPtrCreator<void>
{
  public:
    /// \brief Create an OffsetPtr and return the OffsetPtr and pointed-to address.
    ///
    /// Since the functionality of the OffsetPtr is dependent on the memory region in which it resides, we return a
    /// reference to the OffsetPtr so that it is not copied to the stack.
    template <typename... Args>
    static std::pair<std::reference_wrapper<OffsetPtr<void>>, void*> CreateOffsetPtrInResource(
        ManagedMemoryResource& memory_resource)
    {
        auto* const void_ptr = CreatePointedToObject(memory_resource);
        auto* const ptr_to_offset_ptr = memory_resource.construct<OffsetPtr<void>>(void_ptr);

        SCORE_LANGUAGE_FUTURECPP_ASSERT(ptr_to_offset_ptr != nullptr);
        return {*ptr_to_offset_ptr, void_ptr};
    }

    static void* CreatePointedToObject(ManagedMemoryResource& memory_resource, int initial_value = 0)
    {
        auto* const value_ptr = memory_resource.construct<int>(initial_value);
        return static_cast<void*>(value_ptr);
    }

    static void* GetRawPointer(const OffsetPtr<void>& offset_ptr)
    {
        return offset_ptr.get<int>();
    }
};

template <>
class OffsetPtrCreator<const void>
{
  public:
    /// \brief Create an OffsetPtr and return the OffsetPtr and pointed-to address.
    ///
    /// Since the functionality of the OffsetPtr is dependent on the memory region in which it resides, we return a
    /// reference to the OffsetPtr so that it is not copied to the stack.
    template <typename... Args>
    static std::pair<std::reference_wrapper<OffsetPtr<const void>>, const void*> CreateOffsetPtrInResource(
        ManagedMemoryResource& memory_resource)
    {
        const auto* const void_ptr = CreatePointedToObject(memory_resource);

        auto* const ptr_to_offset_ptr = memory_resource.construct<OffsetPtr<const void>>(void_ptr);

        SCORE_LANGUAGE_FUTURECPP_ASSERT(ptr_to_offset_ptr != nullptr);
        return {*ptr_to_offset_ptr, void_ptr};
    }

    static const void* CreatePointedToObject(ManagedMemoryResource& memory_resource, int initial_value = 0)
    {
        const auto* const value_ptr = memory_resource.construct<const int>(initial_value);
        return static_cast<const void*>(value_ptr);
    }

    static const void* GetRawPointer(const OffsetPtr<const void>& offset_ptr)
    {
        return offset_ptr.get<const int>();
    }
};

template <typename T>
class OffsetPtrMemoryResourceFixture : public ::testing::Test
{
  protected:
    MyBoundedMemoryResource memory_resource_{kDefaultMemoryRegionSize, T::first_type::value};
};

template <typename T>
class OffsetPtrNoBoundsCheckingMemoryResourceFixture : public OffsetPtrMemoryResourceFixture<T>
{
  protected:
    OffsetPtrNoBoundsCheckingMemoryResourceFixture() noexcept
        : memory_resource_{kDefaultMemoryRegionSize, T::first_type::value}
    {
        initial_bounds_checking_value_ = EnableOffsetPtrBoundsChecking(false);
    }

    ~OffsetPtrNoBoundsCheckingMemoryResourceFixture() noexcept
    {
        EnableOffsetPtrBoundsChecking(initial_bounds_checking_value_);
    }

    MyBoundedMemoryResource memory_resource_;
    bool initial_bounds_checking_value_;
};

// Runs 20 combinations:
// - Run each test with a ManagedMemoryResource that is registered with the MemoryResourceRegistry and again with a
//   resource not registered with the MemoryResourceRegistry. In the former case, it will test code paths when an
//   OffsetPtr is storing an offset and the latter case will test when an OffsetPtr is storing an absolute pointer.
// - Run each test with a different type
using AllMemoryResourceAndAllTypeCombinations =
    ::testing::Types<std::pair<UseRegisteredMemoryResource, IntType>,
                     std::pair<UseRegisteredMemoryResource, ConstIntType>,
                     std::pair<UseRegisteredMemoryResource, UInt8Type>,
                     std::pair<UseRegisteredMemoryResource, ConstUInt8Type>,
                     std::pair<UseRegisteredMemoryResource, VeryLargeType>,
                     std::pair<UseRegisteredMemoryResource, ConstVeryLargeType>,
                     std::pair<UseRegisteredMemoryResource, ComplexType>,
                     std::pair<UseRegisteredMemoryResource, ConstComplexType>,
                     std::pair<UseRegisteredMemoryResource, VoidType>,
                     std::pair<UseRegisteredMemoryResource, ConstVoidType>,
                     std::pair<UseUnregisteredMemoryResource, IntType>,
                     std::pair<UseUnregisteredMemoryResource, ConstIntType>,
                     std::pair<UseUnregisteredMemoryResource, UInt8Type>,
                     std::pair<UseUnregisteredMemoryResource, ConstUInt8Type>,
                     std::pair<UseUnregisteredMemoryResource, VeryLargeType>,
                     std::pair<UseUnregisteredMemoryResource, ConstVeryLargeType>,
                     std::pair<UseUnregisteredMemoryResource, ComplexType>,
                     std::pair<UseUnregisteredMemoryResource, ConstComplexType>,
                     std::pair<UseUnregisteredMemoryResource, VoidType>,
                     std::pair<UseUnregisteredMemoryResource, ConstVoidType>>;

// Run each test with a ManagedMemoryResource that is registered with the MemoryResourceRegistry and again with a
// resource not registered with the MemoryResourceRegistry. For now, the main difference is that bounds checking will
// only be done for an OffsetPtr created in a ManagedMemoryResource.
using AllMemoryResourceAndNonVoidTypeCombinations =
    ::testing::Types<std::pair<UseRegisteredMemoryResource, IntType>,
                     std::pair<UseRegisteredMemoryResource, ConstIntType>,
                     std::pair<UseRegisteredMemoryResource, UInt8Type>,
                     std::pair<UseRegisteredMemoryResource, ConstUInt8Type>,
                     std::pair<UseRegisteredMemoryResource, VeryLargeType>,
                     std::pair<UseRegisteredMemoryResource, ConstVeryLargeType>,
                     std::pair<UseRegisteredMemoryResource, ComplexType>,
                     std::pair<UseRegisteredMemoryResource, ConstComplexType>,
                     std::pair<UseUnregisteredMemoryResource, IntType>,
                     std::pair<UseUnregisteredMemoryResource, ConstIntType>,
                     std::pair<UseUnregisteredMemoryResource, UInt8Type>,
                     std::pair<UseUnregisteredMemoryResource, ConstUInt8Type>,
                     std::pair<UseUnregisteredMemoryResource, VeryLargeType>,
                     std::pair<UseUnregisteredMemoryResource, ConstVeryLargeType>,
                     std::pair<UseUnregisteredMemoryResource, ComplexType>,
                     std::pair<UseUnregisteredMemoryResource, ConstComplexType>>;
using AllMemoryResourceAndVoidTypeCombinations =
    ::testing::Types<std::pair<UseRegisteredMemoryResource, VoidType>,
                     std::pair<UseRegisteredMemoryResource, ConstVoidType>,
                     std::pair<UseUnregisteredMemoryResource, VoidType>,
                     std::pair<UseUnregisteredMemoryResource, ConstVoidType>>;

}  // namespace score::memory::shared::test

#endif  // SCORE_LIB_MEMORY_SHARED_TEST_OFFSET_PTR_OFFSET_PTR_TEST_RESOURCES_H
