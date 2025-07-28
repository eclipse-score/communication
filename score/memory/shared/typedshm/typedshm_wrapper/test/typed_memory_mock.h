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
#ifndef SCORE_LIB_MEMORY_SHARED_TYPEDSHM_TYPEDSHM_WRAPPER_TEST_TYPED_MEMORY_MOCK_H
#define SCORE_LIB_MEMORY_SHARED_TYPEDSHM_TYPEDSHM_WRAPPER_TEST_TYPED_MEMORY_MOCK_H

#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"
#include "score/os/errno.h"

#include <cstddef>
#include <cstdint>

#include <gmock/gmock.h>

namespace score::memory::shared
{

class TypedMemoryMock : public score::memory::shared::TypedMemory
{
  public:
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>,
                AllocateNamedTypedMemory,
                (const size_t shm_size, const std::string shm_name, (const permission::UserPermissions& permissions)),
                (const, noexcept, override));

    MOCK_METHOD((score::cpp::expected<int, score::os::Error>),
                AllocateAndOpenAnonymousTypedMemory,
                (const std::uint64_t shm_size),
                (const, noexcept, override));

    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, Unlink, (const std::string shm_name), (const, noexcept, override));
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_TYPEDSHM_TYPEDSHM_WRAPPER_TEST_TYPED_MEMORY_MOCK_H
