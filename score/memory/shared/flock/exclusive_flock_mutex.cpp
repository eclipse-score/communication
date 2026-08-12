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
#include "score/memory/shared/flock/exclusive_flock_mutex.h"

#include "score/os/fcntl.h"

namespace score::memory::shared
{

namespace
{

constexpr auto kLockExclusiveBlocking = os::Fcntl::Operation::kLockExclusive;
constexpr auto kLockExclusiveNonBlocking =
    score::os::Fcntl::Operation::kLockExclusive | score::os::Fcntl::Operation::kLockNB;

}  // namespace

ExclusiveFlockMutex::ExclusiveFlockMutex(const LockFile& lock_file) noexcept
    : exclusive_flock_mutex_{lock_file, kLockExclusiveBlocking, kLockExclusiveNonBlocking}
{
}

}  // namespace score::memory::shared
