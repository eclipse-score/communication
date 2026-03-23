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
#include "score/memory/shared/new_delete_delegate_resource.h"
#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/memory/shared/shared_memory_resource.h"

#include "score/language/safecpp/safe_math/safe_math.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <tuple>
#include <utility>

namespace score::memory::shared
{
namespace  // anonymous
{

// coverity[autosar_cpp14_a0_1_1_violation] false-positive: used in alignment check
constexpr std::uintptr_t PAGE_SIZE{4096U};
static_assert((PAGE_SIZE % alignof(std::max_align_t) == 0), "allocation_buffer_start_address_ is not max aligned!");

}  // namespace

// coverity[autosar_cpp14_a3_3_1_violation] false-positive: declared in header, implemented here
NewDeleteDelegateMemoryResource::NewDeleteDelegateMemoryResource(const std::uint64_t mem_res_id,
                                                                 score::cpp::pmr::memory_resource* upstream_resource) noexcept
    : ManagedMemoryResource{},
      upstream_resource_{upstream_resource},
      memory_resource_id_{mem_res_id},
      proxy_{memory_resource_id_},
      sum_allocated_bytes_{0U},
      current_upstream_allocations_{}
{
    auto result = MemoryResourceRegistry::getInstance().insert_resource({memory_resource_id_, this});
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(result, "memory resource id clash! Inserting NewDeleteDelegateMemoryResource failed.");
}

NewDeleteDelegateMemoryResource::~NewDeleteDelegateMemoryResource()
{
    score::memory::shared::MemoryResourceRegistry::getInstance().remove_resource(memory_resource_id_);
    for (auto& upstream_alloc : current_upstream_allocations_)
    {
        upstream_resource_->deallocate(
            upstream_alloc.first, upstream_alloc.second.bytes, upstream_alloc.second.alignment);
    }
}

void* NewDeleteDelegateMemoryResource::getBaseAddress() const noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-4" rule finding: "reinterpret_cast shall not be used.".
    // This class holds no real memory, it only has a made-up buffer.
    // Thus, we have to come up with arbitrary pointers that represent the size of the buffer.
    // In fact the values are not arbitrary, we start at an aligned pointer that represents one memory page.
    // We stop at the last possible pointer (maximum buffer that would ever be possible)
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): Since above.
    // coverity[autosar_cpp14_a5_2_4_violation]
    return reinterpret_cast<void*>(PAGE_SIZE);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast): see above
}

void* NewDeleteDelegateMemoryResource::getUsableBaseAddress() const noexcept
{
    return getBaseAddress();
}

const void* NewDeleteDelegateMemoryResource::getEndAddress() const noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-4" rule finding: "reinterpret_cast shall not be used.".
    // This class holds no real memory, it only has a made-up buffer.
    // Thus, we have to come up with arbitrary pointers that represent the size of the buffer.
    // In fact the values are not arbitrary, we start at an aligned pointer that represents one memory page.
    // We stop at the last possible pointer (maximum buffer that would ever be possible)
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): Since above.
    // coverity[autosar_cpp14_a5_2_4_violation]
    return reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max());
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast): see above
}

const MemoryResourceProxy* NewDeleteDelegateMemoryResource::getMemoryResourceProxy() noexcept
{
    // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
    // references to private or protected data owned by the class.".
    // The function provides controlled access to the internal 'proxy_' member. 'proxy_' is part of the class's
    // implementation details and is not meant for direct external access.
    //
    // By returning a pointer to 'proxy_', this function serves as an interface to access
    // this member, a common pattern in C++ to maintain encapsulation while exposing necessary internals.
    //
    // Although AUTOSAR C++14 rule A9-3-1 discourages returning addresses of non-static class members,
    // in this context, the design choice is deliberate and justified to provide controlled access,
    // hence the warning is suppressed.
    // coverity[autosar_cpp14_a9_3_1_violation]
    return &proxy_;
}

std::size_t NewDeleteDelegateMemoryResource::GetUserAllocatedBytes() const noexcept
{
    return sum_allocated_bytes_;
}

void* NewDeleteDelegateMemoryResource::do_allocate(const std::size_t bytes, std::size_t alignment)
{
    // the real allocation gets forwarded to upstream new-delete res.
    auto* const result = upstream_resource_->allocate(bytes, alignment);

    // LCOV_EXCL_START (Defensive programming: score::cpp::pmr::memory_resource already asserts that the allocate call on the
    // underlying allocator does not return a nullptr so this branch can never be entered)
    // LCOV_EXCL_BR_START (See line coverage suppression explanation)
    if (result == nullptr)
    {
        score::mw::log::LogError("shm")
            << "DryRunMemoryResource::do_allocate() memory allocation failed! Already allocated bytes: "
            << sum_allocated_bytes_ << ", current allocate request: " << bytes;
        std::terminate();
    }
    // LCOV_EXCL_BR_STOP
    // LCOV_EXCL_STOP

    const auto emplace_result = current_upstream_allocations_.emplace(
        std::piecewise_construct, std::forward_as_tuple(result), std::forward_as_tuple(bytes, alignment));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(emplace_result.second, "Could not emplace allocation in allocation map.");

    // calculation of how many bytes will be effectively needed is done here
    //  at the basis of how our (real) shared-mem resource allocation behavior! So the following alloc-approach exactly
    //  reflects SharedMemoryResource::do_allocate()!

    // In our architecture we have a one-to-one mapping between pointers and integral values.
    // Therefore, casting between the two is well-defined.
    // The resulting pointer is never dereferenced and is used to track the actual amount of allocated memory.
    // NOLINTNEXTLINE(score-banned-function) see above
    void* const start_remaining_allocatable_memory = AddOffsetToPointer(getBaseAddress(), sum_allocated_bytes_);

    // Since GetEndAddress() returns std::numeric_limits<std::uintptr_t>::max(), it can lead to an overflow in pointer
    // subtraction if calculating the memory buffer to pass to std::align (within do_allocation_algorithm) using
    // start_remaining_allocatable_memory and getEndAddress(). Therefore, we pass a theoretical end address which will
    // result in a memory buffer large enough to align start_remaining_allocatable_memory in the worst case. If the
    // pointer is worst-case aligned, then it would require (alignment - 1) bytes of padding.
    auto max_required_padding_result =
        safe_math::Add(bytes, alignment).and_then([](const auto max_required_padding) noexcept {
            return safe_math::Subtract(max_required_padding, 1U);
        });
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(max_required_padding_result.has_value(), "Calculating max required padding overflowed!");

    void* const end_memory_buffer =
        // In our architecture we have a one-to-one mapping between pointers and integral values.
        // Therefore, casting between the two is well-defined.
        // The resulting pointer is never dereferenced and is used to track the actual amount of allocated memory.
        // NOLINTNEXTLINE(score-banned-function) see above
        AddOffsetToPointer(start_remaining_allocatable_memory, max_required_padding_result.value());
    void* const new_aligned_alloc_ptr =
        detail::do_allocation_algorithm(start_remaining_allocatable_memory, end_memory_buffer, bytes, alignment);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(new_aligned_alloc_ptr != nullptr, "Could not align memory address.");
    const auto padding = SubtractPointersBytes(new_aligned_alloc_ptr, start_remaining_allocatable_memory);

    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(padding >= 0);
    const auto allocated_bytes_result =
        safe_math::Add(bytes, static_cast<std::size_t>(padding)).and_then([this](const auto allocated_bytes) noexcept {
            return safe_math::Add(sum_allocated_bytes_, allocated_bytes);
        });
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(allocated_bytes_result.has_value(), "Calculating allocated bytes overflowed!");
    sum_allocated_bytes_ = allocated_bytes_result.value();

    return result;
}

void NewDeleteDelegateMemoryResource::do_deallocate(void* p, std::size_t bytes, std::size_t alignment)
{
    // the real de-allocation gets forwarded to upstream new-delete res.
    upstream_resource_->deallocate(p, bytes, alignment);
    const auto num_erased = current_upstream_allocations_.erase(p);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        num_erased == 1U, "DryRunMemoryResource::do_deallocate() called on an unknown or already deallocated address!");
    // there is nothing more to do as our (real) shared-mem resource doesn't de-allocate as it is a strictly
    // monotonic allocating resource.
}

bool NewDeleteDelegateMemoryResource::do_is_equal(const memory_resource& other) const noexcept
{
    const auto* const other_casted = dynamic_cast<const NewDeleteDelegateMemoryResource*>(&other);
    if (other_casted != nullptr)
    {
        return upstream_resource_->is_equal(*other_casted->upstream_resource_);
    }
    return false;
}

}  // namespace score::memory::shared
