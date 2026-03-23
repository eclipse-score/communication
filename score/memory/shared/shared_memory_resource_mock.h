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
#ifndef SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_RESOURCE_MOCK_H
#define SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_RESOURCE_MOCK_H

#include "score/memory/shared/i_shared_memory_resource.h"
#include <score/memory_resource.hpp>
#include <gmock/gmock.h>

#include <string>

namespace score::memory::shared
{

class SharedMemoryResourceMock : public ISharedMemoryResource
{
  public:
    MOCK_METHOD(MemoryResourceProxy*, getMemoryResourceProxy, (), (noexcept, override));

    MOCK_METHOD(void*, getBaseAddress, (), (const, noexcept, override));

    MOCK_METHOD(void*, getUsableBaseAddress, (), (const, noexcept, override));

    MOCK_METHOD(const void*, getEndAddress, (), (const, noexcept, override));

    MOCK_METHOD(const std::string*, getPath, (), (const, noexcept, override));

    MOCK_METHOD(void, UnlinkFilesystemEntry, (), (const, noexcept, override));

    MOCK_METHOD(std::int32_t, GetFileDescriptor, (), (const, noexcept, override));

    MOCK_METHOD(bool, IsShmInTypedMemory, (), (const, noexcept, override));

    MOCK_METHOD(void*, do_allocate, (const std::size_t bytes, const std::size_t alignment), (override));

    MOCK_METHOD(void, do_deallocate, (void*, std::size_t bytes, std::size_t alignment), (override));

    MOCK_METHOD(bool, do_is_equal, (const ::score::cpp::pmr::memory_resource&), (const, noexcept, override));

    MOCK_METHOD(std::size_t, GetUserAllocatedBytes, (), (const, noexcept, override));

    MOCK_METHOD(bool, IsOffsetPtrBoundsCheckBypassingEnabled, (), (const, noexcept, override));

    MOCK_METHOD(std::string_view, GetIdentifier, (), (const, noexcept, override));
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_RESOURCE_MOCK_H
