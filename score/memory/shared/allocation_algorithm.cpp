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
#include "score/memory/shared/allocation_algorithm.h"

#include "score/memory/shared/pointer_arithmetic_util.h"

#include <memory>

namespace score::memory::shared
{

namespace detail
{

void* do_allocation_algorithm(const void* const alloc_start,
                              const void* const alloc_end,
                              const std::size_t bytes,
                              const std::size_t alignment) noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-3" rule finding: A cast shall not remove any const or volatile
    // qualification from the type of a pointer or reference.
    // Rationale : std::align does not modify the underlying memory of aligned_address
    // (https://timsong-cpp.github.io/cppwp/n4659/ptr.align#lib:align) so the const_cast will not result in undefined
    // behaviour.
    // coverity[autosar_cpp14_a5_2_3_violation]
    void* aligned_address = const_cast<void*>(alloc_start);
    auto buffer_space = static_cast<std::size_t>(SubtractPointersBytes(alloc_end, alloc_start));
    return std::align(alignment, bytes, aligned_address, buffer_space);
}

}  // namespace detail

}  // namespace score::memory::shared
