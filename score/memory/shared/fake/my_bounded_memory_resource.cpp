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
#include "score/memory/shared/fake/my_bounded_memory_resource.h"

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/memory_resource_registry.h"

#include "score/memory/shared/pointer_arithmetic_util.h"

#include <score/assert.hpp>

#include <cstddef>
#include <cstdint>

namespace score::memory::shared::test
{

namespace
{

std::pair<void*, void*> AllocateMemoryRange(const std::size_t memory_resource_size) noexcept
{
    auto* memory_allocation = static_cast<std::uint8_t*>(std::malloc(memory_resource_size));
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(memory_allocation != nullptr, "Malloc must return allocated memory!");
    return {memory_allocation, memory_allocation + memory_resource_size};
}

}  // namespace

std::uint64_t MyBoundedMemoryResource::instanceId = 0U;
std::size_t MyBoundedMemoryResource::memoryResourceProxyAllocationSize_{
    CalculateAlignedSize(sizeof(MemoryResourceProxy), alignof(std::max_align_t))};

MyBoundedMemoryResource::MyBoundedMemoryResource(const std::size_t memory_resource_size,
                                                 const bool register_resource_with_registry)
    : MyBoundedMemoryResource{AllocateMemoryRange(memory_resource_size + memoryResourceProxyAllocationSize_),
                              register_resource_with_registry}
{
    should_free_memory_on_destruction_ = true;
}

/// \brief Construct MyBoundedMemoryResource using an underlying memory region owned by the caller (i.e. will not be
/// created or freed within the lifecyle of this class)
MyBoundedMemoryResource::MyBoundedMemoryResource(const std::pair<void*, void*> memory_range,
                                                 const bool register_resource_with_registry)
    // Suppress "AUTOSAR C++14 M5-2-8" rule finding:  An object with integer type or pointer to void type shall not
    // be converted to an object with pointer type.
    // Rationale : fresh allocated memory needs to be converted to the type that shall be stored in it
    // coverity[autosar_cpp14_m5_2_8_violation]
    : baseAddress_{static_cast<std::uint8_t*>(memory_range.first)},
      currentAddress_{baseAddress_},
      endAddress_{static_cast<std::uint8_t*>(memory_range.second)},
      allocatedMemory_{0U},
      deallocatedMemory_{0U},
      memoryResourceId_{instanceId++},
      manager_{nullptr},
      should_free_memory_on_destruction_{false}
{
    if (register_resource_with_registry)
    {
        const bool registration_result =
            MemoryResourceRegistry::getInstance().insert_resource({memoryResourceId_, this});
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(registration_result, "Could not register memory resource with registry");
    }
    manager_ = AllocateMemoryResourceProxy(memoryResourceId_);
}

MyBoundedMemoryResource::~MyBoundedMemoryResource()
{
    if (should_free_memory_on_destruction_)
    {
        std::free(baseAddress_);
    }
    MemoryResourceRegistry::getInstance().remove_resource(memoryResourceId_);
}

void* MyBoundedMemoryResource::do_allocate(const std::size_t bytes, std::size_t alignment)
{
    void* return_address = currentAddress_;

    const std::size_t max_buffer_size{bytes + alignof(std::max_align_t)};
    std::size_t max_buffer_size_after_padding{max_buffer_size};

    return_address = std::align(alignment, bytes, return_address, max_buffer_size_after_padding);
    const auto alignment_padding_bytes = max_buffer_size - max_buffer_size_after_padding;

    const auto total_allocated_bytes{bytes + alignment_padding_bytes};
    currentAddress_ += total_allocated_bytes;
    allocatedMemory_ += total_allocated_bytes;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(currentAddress_ <= endAddress_,
                           "Current address must be smaller than end address after allocating.");
    return return_address;
}

void MyBoundedMemoryResource::do_deallocate(void* /*memory*/, const std::size_t bytes, std::size_t)
{
    currentAddress_ -= bytes;
    deallocatedMemory_ += bytes;
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(currentAddress_ >= baseAddress_,
                           "Current address must be smaller than end address after allocating.");
}

MemoryResourceProxy* MyBoundedMemoryResource::AllocateMemoryResourceProxy(const std::uint64_t memory_resource_id)
{
    // We allocate the MemoryResourceProxy using worst case alignment so that any further allocations will start at an
    // aligned memory address. This is important so that GetUserAllocatedBytes() is never affected by the allocation of
    // the MemoryResourceProxy.
    auto* storage = do_allocate(memoryResourceProxyAllocationSize_, alignof(std::max_align_t));
    return new (storage) MemoryResourceProxy(memory_resource_id);
}

}  // namespace score::memory::shared::test
