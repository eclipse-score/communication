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
#ifndef SCORE_LIB_MEMORY_SHARED_ALLOCATION_ALGORITHM_H
#define SCORE_LIB_MEMORY_SHARED_ALLOCATION_ALGORITHM_H

#include <cstddef>

namespace score::memory::shared
{

namespace detail
{

/// \brief Implementation of a simplistic monotonic allocation algo as used by do_allocate().
/// \param alloc_start address where allocation can start (start of free buffer space)
/// \param alloc_end address where allocation shall end (end of free buffer space)
/// \param bytes how many bytes to allocate
/// \param alignment
/// \return If allocation is successful a valid pointer is returned, otherwise a nullptr
void* do_allocation_algorithm(const void* const alloc_start,
                              const void* const alloc_end,
                              const std::size_t bytes,
                              const std::size_t alignment) noexcept;

}  // namespace detail

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_ALLOCATION_ALGORITHM_H
