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
#ifndef SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_RESOURCE_HEAP_ALLOCATOR_MOCK_H
#define SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_RESOURCE_HEAP_ALLOCATOR_MOCK_H

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/new_delete_delegate_resource.h"

#include <gmock/gmock.h>
#include <cstdint>
#include <string>

namespace score::memory::shared
{

class SharedMemoryResourceHeapAllocatorMock : public ISharedMemoryResource
{
  public:
    explicit SharedMemoryResourceHeapAllocatorMock(const std::uint64_t mem_res_id) : resource_{mem_res_id} {}

    MOCK_METHOD(void*, getBaseAddress, (), (const, noexcept, override));

    MOCK_METHOD(void*, getUsableBaseAddress, (), (const, noexcept, override));

    MOCK_METHOD(const void*, getEndAddress, (), (const, noexcept, override));

    MOCK_METHOD(const std::string*, getPath, (), (const, noexcept, override));

    MOCK_METHOD(void, UnlinkFilesystemEntry, (), (const, noexcept, override));

    MOCK_METHOD(std::int32_t, GetFileDescriptor, (), (const, noexcept, override));

    MOCK_METHOD(bool, IsShmInTypedMemory, (), (const, noexcept, override));

    MOCK_METHOD(bool, IsOffsetPtrBoundsCheckBypassingEnabled, (), (const, noexcept, override));

    MOCK_METHOD(std::string_view, GetIdentifier, (), (const, noexcept, override));

    const MemoryResourceProxy* getMemoryResourceProxy() noexcept override
    {
        return resource_.getMemoryResourceProxy();
    }

    std::size_t GetUserAllocatedBytes() const noexcept override
    {
        return resource_.GetUserAllocatedBytes();
    }

  private:
    void* do_allocate(const std::size_t bytes, const std::size_t alignment) override
    {
        return resource_.allocate(bytes, alignment);
    }

    void do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) override
    {
        resource_.deallocate(ptr, bytes, alignment);
    }

    bool do_is_equal(const memory_resource& other) const noexcept override
    {
        return resource_.is_equal(other);
    }

    NewDeleteDelegateMemoryResource resource_;
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_RESOURCE_HEAP_ALLOCATOR_MOCK_H
