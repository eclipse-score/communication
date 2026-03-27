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
#ifndef SCORE_LIB_MEMORY_SHARED_SEALEDSHM_SEALEDSHM_WRAPPER_SEALED_SHM_MOCK_H
#define SCORE_LIB_MEMORY_SHARED_SEALEDSHM_SEALEDSHM_WRAPPER_SEALED_SHM_MOCK_H

#include "score/expected.hpp"
#include "score/memory/shared/sealedshm/sealedshm_wrapper/i_sealed_shm.h"
#include "score/os/errno.h"
#include <cstdint>

#include <gmock/gmock.h>

namespace score::memory::shared
{

class SealedShmMock : public score::memory::shared::ISealedShm
{
  public:
    MOCK_METHOD((score::cpp::expected<std::int32_t, score::os::Error>),
                OpenAnonymous,
                (const mode_t mode),
                (noexcept, override));

    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, Seal, (int fd, std::uint64_t size), (noexcept, override));
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SEALEDSHM_SEALEDSHM_WRAPPER_SEALED_SHM_MOCK_H
