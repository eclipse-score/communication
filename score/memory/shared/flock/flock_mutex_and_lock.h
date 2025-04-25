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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_FLOCK_FLOCK_MUTEX_AND_LOCK_H
#define BASELIBS_SCORE_MEMORY_SHARED_FLOCK_FLOCK_MUTEX_AND_LOCK_H

#include "score/memory/shared/lock_file.h"

#include <mutex>

namespace score::memory::shared
{

template <typename T>
class FlockMutexAndLock final
{
  public:
    explicit FlockMutexAndLock(const memory::shared::LockFile& lock_file) noexcept
        : mutex_{lock_file}, lock_{mutex_, std::defer_lock}
    {
    }
    ~FlockMutexAndLock() noexcept = default;

    bool TryLock() noexcept
    {
        return lock_.try_lock();
    }

    // Since lock_ stores a reference to mutex_, we should not
    // move the FlockMutexAndLock as it will invalidate this reference.
    FlockMutexAndLock(const FlockMutexAndLock&) = delete;
    FlockMutexAndLock& operator=(const FlockMutexAndLock&) = delete;
    FlockMutexAndLock(FlockMutexAndLock&&) = delete;
    FlockMutexAndLock& operator=(FlockMutexAndLock&&) = delete;

  private:
    T mutex_;
    std::unique_lock<T> lock_;
};

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_FLOCK_FLOCK_MUTEX_AND_LOCK_H
