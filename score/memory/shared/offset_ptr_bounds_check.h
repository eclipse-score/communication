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
#ifndef SCORE_LIB_MEMORY_SHARED_OFFSET_PTR_BOUNDS_CHECK_H
#define SCORE_LIB_MEMORY_SHARED_OFFSET_PTR_BOUNDS_CHECK_H

#include "score/memory/shared/memory_region_bounds.h"
#include "score/memory/shared/memory_resource_registry.h"

#include <cstddef>
#include <optional>

namespace score::memory::shared
{

bool DoesOffsetPtrInSharedMemoryPassBoundsChecks(const void* const offset_ptr_address,
                                                 const std::ptrdiff_t offset,
                                                 const MemoryRegionBounds& offset_ptr_memory_bounds,
                                                 const std::size_t pointed_type_size,
                                                 const std::size_t offset_ptr_size) noexcept;

bool DoesOffsetPtrNotInSharedMemoryPassBoundsChecks(const void* const offset_ptr_address,
                                                    const std::ptrdiff_t offset,
                                                    const MemoryRegionBounds& offset_ptr_memory_bounds,
                                                    const std::size_t pointed_type_size,
                                                    const std::size_t offset_ptr_size) noexcept;

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_OFFSET_PTR_BOUNDS_CHECK_H
