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
#ifndef SCORE_LIB_MEMORY_SHARED_TYPEDSHM_UTILS_TYPED_MEMORY_UTILS_H
#define SCORE_LIB_MEMORY_SHARED_TYPEDSHM_UTILS_TYPED_MEMORY_UTILS_H

#include <sys/types.h>
#include <optional>

namespace score::memory::shared
{
/// \brief Acquires the UID of the typed memory daemon process.
/// \return The UID of the typed memory daemon if available, std::nullopt otherwise.
std::optional<uid_t> AcquireTypedMemoryDaemonUid() noexcept;

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_TYPEDSHM_UTILS_TYPED_MEMORY_UTILS_H
