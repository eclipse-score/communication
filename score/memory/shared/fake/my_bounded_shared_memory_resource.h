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
#ifndef SCORE_LIB_MEMORY_SHARED_FAKE_MY_BOUNDED_SHARED_MEMORY_RESOURCE_H
#define SCORE_LIB_MEMORY_SHARED_FAKE_MY_BOUNDED_SHARED_MEMORY_RESOURCE_H

#include "score/memory/shared/fake/my_bounded_memory_resource.h"
#include "score/memory/shared/i_shared_memory_resource.h"

namespace score::memory::shared::test
{

class MyBoundedSharedMemoryResource final : public ISharedMemoryResource
{
  public:
    /// \brief Construct MyBoundedSharedMemoryResource which owns the underlying memory region (i.e. will create and
    /// free it within the lifecyle of this class)
    MyBoundedSharedMemoryResource(const std::size_t memory_resource_size = 200U,
                                  const bool register_resource_with_registry = true);

    /// \brief Construct MyBoundedSharedMemoryResource using an underlying memory region owned by the caller (i.e. will
    /// not be created or freed within the lifecyle of this class)
    MyBoundedSharedMemoryResource(const std::pair<void*, void*> memory_range,
                                  const bool register_resource_with_registry = true);

    const std::string* getPath() const noexcept override
    {
        return nullptr;
    }

    std::string_view GetIdentifier() const noexcept override
    {
        return "id: 123";
    };

    void UnlinkFilesystemEntry() const noexcept override {}

    FileDescriptor GetFileDescriptor() const noexcept override
    {
        return FileDescriptor{1};
    }

    bool IsShmInTypedMemory() const noexcept override
    {
        return false;
    }

    MemoryResourceProxy* getMemoryResourceProxy() noexcept override
    {
        return resource_.getMemoryResourceProxy();
    }

    void* getBaseAddress() const noexcept override
    {
        return resource_.getBaseAddress();
    }

    void* getUsableBaseAddress() const noexcept override
    {
        return resource_.getUsableBaseAddress();
    }

    const void* getEndAddress() const noexcept override
    {
        return resource_.getEndAddress();
    }

    size_t getAllocatedMemory() const
    {
        return resource_.getAllocatedMemory();
    };

    std::size_t GetUserAllocatedBytes() const noexcept override
    {
        return resource_.GetUserAllocatedBytes();
    };

    std::size_t GetUserDeAllocatedBytes() const noexcept
    {
        return resource_.GetUserDeAllocatedBytes();
    }

    bool IsOffsetPtrBoundsCheckBypassingEnabled() const noexcept override
    {
        return resource_.IsOffsetPtrBoundsCheckBypassingEnabled();
    }

    std::uint64_t getMemoryResourceId() const
    {
        return resource_.getMemoryResourceId();
    };

  private:
    void* do_allocate(const std::size_t bytes, std::size_t alignment) override
    {
        return resource_.do_allocate(bytes, alignment);
    }

    void do_deallocate(void* memory, const std::size_t bytes, std::size_t alignment) override
    {
        resource_.do_deallocate(memory, bytes, alignment);
    }

    bool do_is_equal(const memory_resource& resource) const noexcept override
    {
        return resource_.do_is_equal(resource);
    }

    MyBoundedMemoryResource resource_;
};

}  // namespace score::memory::shared::test

#endif  // SCORE_LIB_MEMORY_SHARED_FAKE_MY_BOUNDED_SHARED_MEMORY_RESOURCE_H
