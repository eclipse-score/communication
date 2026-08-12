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
#ifndef SCORE_LIB_MEMORY_SHARED_TEST_OFFSET_PTR_BOUNDS_CHECK_MEMORY_POOL_H
#define SCORE_LIB_MEMORY_SHARED_TEST_OFFSET_PTR_BOUNDS_CHECK_MEMORY_POOL_H

#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/memory/shared/test_offset_ptr/offset_ptr_test_resources.h"

#include <score/assert.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace score::memory::shared::test
{

template <typename PointedType>
class BoundsCheckMemoryPool
{
  public:
    // Memory pool of 400 bytes. The bytes 112 -> 200 are registered with the MemoryResourceRegistry (The specific
    // numbers are chosen to respect alignment of PointedType / OffsetPtr<PointedType>). This range will be used for
    // bounds checking. We use a larger memory pool than that registered with the MemoryResourceRegistry to allow fine
    // grained control of where OffsetPtrs and the pointed-to objects are created for testing e.g. we can create an
    // OffsetPtr within the region and point to an address such that the created object overlaps the memory region
    // boundary which should then fail during bounds checking.
    using MemoryPool = std::array<std::uint8_t, 400>;

    void Reset()
    {
        data_region_.fill(0);
    }

    MemoryPool::iterator GetStartOfValidRegion() const noexcept
    {
        return start_of_valid_region_;
    }
    MemoryPool::iterator GetEndOfValidRegion() const noexcept
    {
        return end_of_valid_region_;
    }

    MemoryPool::iterator GetPointedToAddressInValidRange() noexcept
    {
        return start_of_valid_region_;
    }

    MemoryPool::iterator GetPointedToAddressBeforeValidRange() noexcept
    {
        return data_region_.begin();
    }

    MemoryPool::iterator GetPointedToAddressAfterValidRange() noexcept
    {
        return end_of_valid_region_ + 64U;
    }

    MemoryPool::iterator GetPointedToAddressOverlappingWithStartRange() noexcept
    {
        const auto half_pointed_to_aligned_size = CalculateAlignedSize(sizeof(PointedType) / 2, alignof(PointedType));
        static_assert((half_pointed_to_aligned_size < sizeof(PointedType)));
        return start_of_valid_region_ - half_pointed_to_aligned_size;
    }

    MemoryPool::iterator GetPointedToAddressOverlappingWithEndRange() noexcept
    {
        const auto half_pointed_to_aligned_size = CalculateAlignedSize(sizeof(PointedType) / 2, alignof(PointedType));
        static_assert((half_pointed_to_aligned_size < sizeof(PointedType)));
        return end_of_valid_region_ - half_pointed_to_aligned_size;
    }

    /// \brief Gets the start address of the pointed-to object such that its end address would be equal to
    /// getEndAddress().
    ///
    /// The memory address returned by getEndAddress is a past-the-end address. Therefore, if the object exists here,
    /// dereferencing / getting will terminate.
    MemoryPool::iterator GetPointedToAddressFinishingAtEndAddress() noexcept
    {
        return end_of_valid_region_ - (sizeof(PointedType));
    }

    MemoryPool::iterator GetOffsetPtrAddressInValidRange() noexcept
    {
        return GetPointedToAddressInValidRange() + kOffsetPtrFromPointedToAddressBuffer;
    }

    MemoryPool::iterator GetSecondOffsetPtrAddressInValidRange() noexcept
    {
        return GetOffsetPtrAddressInValidRange() + kSecondOffsetPtrFromFirstOffsetPtrBuffer;
    }

    MemoryPool::iterator GetThirdOffsetPtrAddressInValidRange() noexcept
    {
        return GetSecondOffsetPtrAddressInValidRange() + kThirdOffsetPtrFromSecondOffsetPtrBuffer;
    }

    /// \brief Gets the start address of the OffsetPtr such that its end address would be equal to getEndAddress().
    MemoryPool::iterator GetOffsetPtrAddressFinishingAtEndAddress() noexcept
    {
        return end_of_valid_region_ - sizeof(OffsetPtr<PointedType>);
    }

    MemoryPool::iterator GetOffsetPtrAddressBeforeValidRange() noexcept
    {
        return GetPointedToAddressBeforeValidRange() + kOffsetPtrFromPointedToAddressBuffer;
    }

    MemoryPool::iterator GetOffsetPtrAddressAfterValidRange() noexcept
    {
        return GetPointedToAddressAfterValidRange() + kOffsetPtrFromPointedToAddressBuffer;
    }

    MemoryPool::iterator GetOffsetPtrAddressOverlappingWithStartRange() noexcept
    {
        const auto half_offset_ptr_aligned_size =
            CalculateAlignedSize(sizeof(OffsetPtr<PointedType>) / 2, alignof(OffsetPtr<PointedType>));
        static_assert((half_offset_ptr_aligned_size < sizeof(OffsetPtr<PointedType>)));
        return start_of_valid_region_ - half_offset_ptr_aligned_size;
    }

    MemoryPool::iterator GetOffsetPtrAddressOverlappingWithEndRange() noexcept
    {
        const auto half_offset_ptr_aligned_size =
            CalculateAlignedSize(sizeof(OffsetPtr<PointedType>) / 2, alignof(OffsetPtr<PointedType>));
        static_assert((half_offset_ptr_aligned_size < sizeof(OffsetPtr<PointedType>)));
        return end_of_valid_region_ - half_offset_ptr_aligned_size;
    }

  private:
    // We use this buffer to ensure that the address provided for the pointed-to address never overlaps with the address
    // of the OffsetPtr itself.
    static constexpr std::size_t kOffsetPtrFromPointedToAddressBuffer{
        CalculateAlignedSize(sizeof(PointedType) + 8U, alignof(OffsetPtr<PointedType>))};
    static constexpr std::size_t kSecondOffsetPtrFromFirstOffsetPtrBuffer{kOffsetPtrFromPointedToAddressBuffer};
    static constexpr std::size_t kThirdOffsetPtrFromSecondOffsetPtrBuffer{kOffsetPtrFromPointedToAddressBuffer};

    alignas(std::max_align_t) MemoryPool data_region_{};
    MemoryPool::iterator start_of_valid_region_{data_region_.begin() + 112};
    MemoryPool::iterator end_of_valid_region_{data_region_.begin() + 200};
};

template <typename T>
OffsetPtr<T>* CreateOffsetPtr(const typename BoundsCheckMemoryPool<T>::MemoryPool::iterator offset_ptr_address,
                              const typename BoundsCheckMemoryPool<T>::MemoryPool::iterator pointed_to_address) noexcept
{
    // auto* const pointed_to_object = new (pointed_to_address) T(10);
    void* const v = reinterpret_cast<void*>(pointed_to_address);
    auto* const p = static_cast<T*>(v);
    return new (offset_ptr_address) OffsetPtr<T>(p);
}

template <>
OffsetPtr<void>* CreateOffsetPtr<void>(
    const typename BoundsCheckMemoryPool<void>::MemoryPool::iterator offset_ptr_address,
    const typename BoundsCheckMemoryPool<void>::MemoryPool::iterator pointed_to_address) noexcept;

/// \brief Helper class for resetting the state of a BoundsCheckMemoryPool
///
/// The BoundsCheckMemoryPool is RAII. However, if the object is stored in a static context, the memory pool will
/// persist between tests, and the resource won't be cleaned up until the test process finishes. This class can provide
/// an RAII wrapper that can be used to reset the memory pool between tests.
template <typename PointedType>
class BoundsCheckMemoryPoolGuard
{
  public:
    explicit BoundsCheckMemoryPoolGuard(BoundsCheckMemoryPool<PointedType>& mem_pool) noexcept : mem_pool_{mem_pool} {}

    ~BoundsCheckMemoryPoolGuard() noexcept
    {
        mem_pool_.Reset();
    }

    BoundsCheckMemoryPoolGuard(const BoundsCheckMemoryPoolGuard&) = delete;
    BoundsCheckMemoryPoolGuard& operator=(const BoundsCheckMemoryPoolGuard&) = delete;
    BoundsCheckMemoryPoolGuard(BoundsCheckMemoryPoolGuard&&) = delete;
    BoundsCheckMemoryPoolGuard& operator=(BoundsCheckMemoryPoolGuard&&) = delete;

  private:
    BoundsCheckMemoryPool<PointedType>& mem_pool_;
};

}  // namespace score::memory::shared::test

#endif  // SCORE_LIB_MEMORY_SHARED_TEST_OFFSET_PTR_BOUNDS_CHECK_MEMORY_POOL_H
