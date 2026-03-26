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
#include "score/memory/shared/flock/shared_flock_mutex.h"

#include "score/os/fcntl.h"

namespace score::memory::shared
{

namespace
{

constexpr auto kLockSharedBlocking = os::Fcntl::Operation::kLockShared;
constexpr auto kLockSharedNonBlocking = score::os::Fcntl::Operation::kLockShared | score::os::Fcntl::Operation::kLockNB;

}  // namespace

SharedFlockMutex::SharedFlockMutex(const LockFile& lock_file) noexcept
    : shared_flock_mutex_{lock_file, kLockSharedBlocking, kLockSharedNonBlocking}
{
}

}  // namespace score::memory::shared
