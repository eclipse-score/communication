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
#ifndef SCORE_LIB_MEMORY_SHARED_FAKE_MYBOUNDEDMEMORYRESOURCE_H
#define SCORE_LIB_MEMORY_SHARED_FAKE_MYBOUNDEDMEMORYRESOURCE_H

#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <cstddef>
#include <cstdint>

namespace score::memory::shared::test
{

class MyBoundedMemoryResource final : public ManagedMemoryResource
{
    friend class MyBoundedSharedMemoryResource;

  public:
    /// \brief Construct MyBoundedMemoryResource which owns the underlying memory region (i.e. will create and free it
    /// within the lifecyle of this class)
    MyBoundedMemoryResource(const std::size_t memory_resource_size = 200U,
                            const bool register_resource_with_registry = true);

    /// \brief Construct MyBoundedMemoryResource using an underlying memory region owned by the caller (i.e. will not be
    /// created or freed within the lifecyle of this class)
    MyBoundedMemoryResource(const std::pair<void*, void*> memory_range,
                            const bool register_resource_with_registry = true);

    ~MyBoundedMemoryResource() final;

    MyBoundedMemoryResource(const MyBoundedMemoryResource&) noexcept = default;
    MyBoundedMemoryResource(MyBoundedMemoryResource&&) noexcept = default;
    MyBoundedMemoryResource& operator=(const MyBoundedMemoryResource&) noexcept = default;
    MyBoundedMemoryResource& operator=(MyBoundedMemoryResource&&) noexcept = default;

    MemoryResourceProxy* getMemoryResourceProxy() noexcept override
    {
        return manager_;
    }

    void* getBaseAddress() const noexcept override
    {
        return baseAddress_;
    }

    void* getUsableBaseAddress() const noexcept override
    {
        // Memory is allocated on construction to store a MemoryResourceProxy in the memory region. This is part of the
        // "ControlBlock" of the memory region and therefore set the BaseAddress after it.
        return AddOffsetToPointer(baseAddress_, memoryResourceProxyAllocationSize_);
    }

    const void* getEndAddress() const noexcept override
    {
        return endAddress_;
    }

    size_t getAllocatedMemory() const
    {
        return already_allocated_bytes_;
    };

    std::size_t GetUserAllocatedBytes() const noexcept override
    {
        // Memory is allocated on construction to store a MemoryResourceProxy in the memory region. This is part of the
        // "ControlBlock" of the memory region and therefore we don't take it into account in the number of user
        // allocated bytes.
        return already_allocated_bytes_ - memoryResourceProxyAllocationSize_;
    };

    std::size_t GetUserDeAllocatedBytes() const noexcept
    {
        return deallocatedMemory_;
    }

    bool IsOffsetPtrBoundsCheckBypassingEnabled() const noexcept override
    {
        return false;
    }

    std::uint64_t getMemoryResourceId() const
    {
        return memoryResourceId_;
    };

  private:
    void* do_allocate(const std::size_t bytes, std::size_t) override;

    void do_deallocate(void* /*memory*/, const std::size_t bytes, std::size_t) override;

    bool do_is_equal(const memory_resource&) const noexcept override
    {
        return false;
    }

    MemoryResourceProxy* AllocateMemoryResourceProxy(const std::uint64_t memory_resource_id);

    static std::uint64_t instanceId;
    static std::size_t memoryResourceProxyAllocationSize_;

    void* baseAddress_;
    void* endAddress_;
    std::size_t virtual_address_space_to_reserve_;
    std::size_t already_allocated_bytes_;
    std::size_t deallocatedMemory_;
    std::uint64_t memoryResourceId_;
    MemoryResourceProxy* manager_;
    bool should_free_memory_on_destruction_;
};

}  // namespace score::memory::shared::test

#endif  // SCORE_LIB_MEMORY_SHARED_FAKE_MYBOUNDEDMEMORYRESOURCE_H
