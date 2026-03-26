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
#ifndef SCORE_LIB_MEMORY_SHARED_SEALEDSHM_SEALEDSHM_WRAPPER_I_SEALED_SHM_H
#define SCORE_LIB_MEMORY_SHARED_SEALEDSHM_SEALEDSHM_WRAPPER_I_SEALED_SHM_H

#include "score/expected.hpp"
#include "score/os/errno.h"
#include <cstdint>

namespace score::memory::shared
{

class ISealedShm
{
  public:
    virtual ~ISealedShm() = default;
    ISealedShm(const ISealedShm&) noexcept = delete;
    ISealedShm& operator=(const ISealedShm&) & noexcept = delete;

    /**
     * @brief Create and open an anonymous shared memory object.
     * @param mode the permissions for the shared memory object.
     * @return filedescriptor on success, score::os::Error on error.
     */
    virtual score::cpp::expected<std::int32_t, score::os::Error> OpenAnonymous(const mode_t mode) noexcept = 0;

    /**
     * @brief Seal the given shared memory object.
     * @param fd A file descriptor referencing the shared memory object to be sealed.
     * @param size The fixed size for the sealed shared memory object.
     * @return score::cpp::blank on success, score::os::Error on error.
     */
    virtual score::cpp::expected_blank<score::os::Error> Seal(int fd, std::uint64_t size) noexcept = 0;

  protected:
    ISealedShm() = default;
    ISealedShm(ISealedShm&&) noexcept = default;
    ISealedShm& operator=(ISealedShm&&) & noexcept = default;
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SEALEDSHM_SEALEDSHM_WRAPPER_I_SEALED_SHM_H
