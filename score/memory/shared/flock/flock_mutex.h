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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_FLOCK_FLOCK_MUTEX_H
#define BASELIBS_SCORE_MEMORY_SHARED_FLOCK_FLOCK_MUTEX_H

#include "score/memory/shared/lock_file.h"

#include "score/os/fcntl.h"

namespace score::memory::shared
{

class FlockMutex
{
  public:
    FlockMutex(const LockFile& lock_file,
               const score::os::Fcntl::Operation locking_operation,
               const score::os::Fcntl::Operation try_locking_operation) noexcept;

    void lock() noexcept;
    bool try_lock() noexcept;
    void unlock() noexcept;

  private:
    std::int32_t file_descriptor_;
    score::os::Fcntl::Operation locking_operation_;
    score::os::Fcntl::Operation try_locking_operation_;
};

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_FLOCK_FLOCK_MUTEX_H
