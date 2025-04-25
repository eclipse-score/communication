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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_FAKE_MYMEMORYRESOURCE_H
#define BASELIBS_SCORE_MEMORY_SHARED_FAKE_MYMEMORYRESOURCE_H

#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <score/utility.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace score::memory::shared::test
{

class MyMemoryResource : public ManagedMemoryResource
{
  public:
    // Set the default memory range to cover all possible memory addresses so that bounds checking of the
    // MemoryResourceProxy is guaranteed to pass. We use start address 1 so that (getBaseAddress() == nullptr) returns
    // false. In tests where bounds checking should be considered, use MyBoundedMemoryResource.
    MyMemoryResource(const std::pair<std::uintptr_t, std::uintptr_t> memory_range =
                         {std::uintptr_t{1U}, std::numeric_limits<std::uintptr_t>::max()}) noexcept
        : baseAddress_{CastIntegerToPointer<void*>(memory_range.first)},
          endAddress_{CastIntegerToPointer<void*>(memory_range.second)},
          allocationPossible_{true},
          allocatedMemory_{0U},
          memoryResourceId_{instanceId++},
          manager_{memoryResourceId_}
    {
    }

    MemoryResourceProxy* getMemoryResourceProxy() noexcept override
    {
        MemoryResourceRegistry::getInstance().clear();
        score::cpp::ignore = MemoryResourceRegistry::getInstance().insert_resource({memoryResourceId_, this});
        return &this->manager_;
    }

    void* getBaseAddress() const noexcept override
    {
        return baseAddress_;
    }

    void* getUsableBaseAddress() const noexcept override
    {
        return baseAddress_;
    }

    std::size_t GetUserAllocatedBytes() const noexcept override
    {
        return allocatedMemory_;
    };

    const void* getEndAddress() const noexcept override
    {
        return endAddress_;
    }

    bool IsOffsetPtrBoundsCheckBypassingEnabled() const noexcept override
    {
        return true;
    }

    size_t getAllocatedMemory() const
    {
        return allocatedMemory_;
    };

    std::uint64_t getMemoryResourceId() const
    {
        return memoryResourceId_;
    };

    bool isAllocationPossible() const
    {
        return allocationPossible_;
    };

    void setAllocationPossible(const bool allocation_possible)
    {
        allocationPossible_ = allocation_possible;
    };

  private:
    void* do_allocate(const std::size_t bytes, std::size_t) override
    {
        if (allocationPossible_)
        {
            allocatedMemory_ += bytes;
            return std::malloc(bytes);
        }
        else
        {
            throw std::bad_alloc{};
        }
    }

    void do_deallocate(void* memory, const std::size_t bytes, std::size_t) override
    {
        allocatedMemory_ -= bytes;
        std::free(memory);
    }

    bool do_is_equal(const memory_resource&) const noexcept override
    {
        return false;
    }

    static std::uint64_t instanceId;

    void* const baseAddress_;
    void* const endAddress_;
    bool allocationPossible_;
    std::size_t allocatedMemory_;
    const std::uint64_t memoryResourceId_;
    MemoryResourceProxy manager_;
};

}  // namespace score::memory::shared::test

#endif  // BASELIBS_SCORE_MEMORY_SHARED_FAKE_MYMEMORYRESOURCE_H
