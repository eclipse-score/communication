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
#include "score/memory/shared/flock/flock_mutex.h"

#include "score/os/errno.h"
#include "score/mw/log/logging.h"

#include <exception>

namespace score::memory::shared
{

FlockMutex::FlockMutex(const LockFile& lock_file,
                       const score::os::Fcntl::Operation locking_operation,
                       const score::os::Fcntl::Operation try_locking_operation) noexcept
    : file_descriptor_{lock_file.file_descriptor_},
      locking_operation_{locking_operation},
      try_locking_operation_{try_locking_operation}
{
}

void FlockMutex::lock() noexcept
{
    auto flock_result = score::os::Fcntl::instance().flock(file_descriptor_, locking_operation_);
    if (!flock_result.has_value())
    {
        score::mw::log::LogFatal("shm") << "Flock locking operation failed:" << flock_result.error().ToString();
        std::terminate();
    }
}

bool FlockMutex::try_lock() noexcept
{
    auto flock_result = score::os::Fcntl::instance().flock(file_descriptor_, try_locking_operation_);
    if (!flock_result.has_value())
    {
        if (flock_result.error() == ::score::os::Error::createFromErrnoFlockSpecific(EWOULDBLOCK))
        {
            return false;
        }
        score::mw::log::LogFatal("shm") << "Flock try locking operation failed:" << flock_result.error().ToString();
        std::terminate();
    }
    return true;
}

void FlockMutex::unlock() noexcept
{
    constexpr auto unlocking_operation = score::os::Fcntl::Operation::kUnLock;
    auto flock_result = score::os::Fcntl::instance().flock(file_descriptor_, unlocking_operation);
    if (!flock_result.has_value())
    {
        score::mw::log::LogFatal("shm") << "Flock unlocking operation failed:" << flock_result.error().ToString();
        std::terminate();
    }
}

}  // namespace score::memory::shared
