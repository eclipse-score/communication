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
#ifndef SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_FACTORY_MOCK_H
#define SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_FACTORY_MOCK_H

#include "score/memory/shared/i_shared_memory_factory.h"
#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"

#include <memory>
#include <string>

#include <gmock/gmock.h>

namespace score::memory::shared
{

class SharedMemoryFactoryMock : public ISharedMemoryFactory
{
  public:
    MOCK_METHOD(std::shared_ptr<ISharedMemoryResource>,
                Open,
                (const std::string&, const bool, const std::optional<score::cpp::v1::span<const uid_t>>&),
                (noexcept, override));

    MOCK_METHOD(std::shared_ptr<ISharedMemoryResource>,
                Create,
                (std::string, InitializeCallback, const std::size_t, const UserPermissions&, const bool),
                (noexcept, override));

    MOCK_METHOD(std::shared_ptr<ISharedMemoryResource>,
                CreateAnonymous,
                (std::uint64_t, InitializeCallback, const std::size_t, const UserPermissions&, const bool),
                (noexcept, override));

    MOCK_METHOD(std::shared_ptr<ISharedMemoryResource>,
                CreateOrOpen,
                (std::string, InitializeCallback, const std::size_t, const AccessControl, const bool),
                (noexcept, override));

    MOCK_METHOD(void, Remove, (const std::string&), (noexcept, override));

    MOCK_METHOD(void, RemoveStaleArtefacts, (const std::string&), (noexcept, override));

    MOCK_METHOD(void,
                SetTypedMemoryProvider,
                (std::shared_ptr<score::memory::shared::TypedMemory>),
                (noexcept, override));

    MOCK_METHOD(std::size_t, GetControlBlockSize, (), (noexcept, override));

    MOCK_METHOD(void, Clear, (), (noexcept, override));
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_FACTORY_MOCK_H
