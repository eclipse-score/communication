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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_FLOCK_EXCLUSIVE_FLOCK_MUTEX_H
#define BASELIBS_SCORE_MEMORY_SHARED_FLOCK_EXCLUSIVE_FLOCK_MUTEX_H

#include "score/memory/shared/flock/flock_mutex.h"
#include "score/memory/shared/lock_file.h"

namespace score::memory::shared
{

class ExclusiveFlockMutex
{
  public:
    explicit ExclusiveFlockMutex(const LockFile& lock_file) noexcept;

    void lock() noexcept
    {
        exclusive_flock_mutex_.lock();
    }
    bool try_lock() noexcept
    {
        return exclusive_flock_mutex_.try_lock();
    }
    void unlock() noexcept
    {
        exclusive_flock_mutex_.unlock();
    }

  private:
    FlockMutex exclusive_flock_mutex_;
};

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_FLOCK_EXCLUSIVE_FLOCK_MUTEX_H
