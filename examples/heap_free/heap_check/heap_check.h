/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_EXAMPLES_HEAP_CHECK_HEAP_CHECK_H
#define SCORE_MW_COM_EXAMPLES_HEAP_CHECK_HEAP_CHECK_H

// Include this file in exactly ONE translation unit per binary (main.cpp only).
//
// Replaces the global operator new with a version that aborts if the calling
// thread allocates after forbid_heap() has been called on that thread.
// Check is per-thread: library background threads may still allocate freely.

#include <cstdlib>
#include <new>

namespace heap_check
{

// Per-thread phase flag. false = init/cleanup (heap OK), true = operational (heap forbidden).
// Only the thread that calls forbid_heap() is constrained.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): thread_local; intentional mutable state
inline thread_local bool tl_heap_forbidden{false};

// Call once from the application thread at the init/operational boundary.
inline void forbid_heap() noexcept
{
    tl_heap_forbidden = true;
}

// Call once from the application thread before entering cleanup/shutdown.
// After this, heap allocations are permitted again on this thread.
inline void allow_heap() noexcept
{
    tl_heap_forbidden = false;
}

}  // namespace heap_check

// NOLINTNEXTLINE(misc-new-delete-overloads): intentional process-global override for heap checking
void* operator new(std::size_t sz)
{
    if (heap_check::tl_heap_forbidden)
    {
        std::abort();
    }
    void* ptr = std::malloc(sz);  // NOLINT(cppcoreguidelines-no-malloc): malloc is correct inside operator new
    if (ptr == nullptr)
    {
        throw std::bad_alloc{};
    }
    return ptr;
}

// NOLINTNEXTLINE(misc-new-delete-overloads): matching override required by the C++ standard
void* operator new[](std::size_t sz)
{
    return ::operator new(sz);
}

// NOLINTNEXTLINE(misc-new-delete-overloads): matching override required by the C++ standard
void operator delete(void* ptr) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): matching override required by the C++ standard
void operator delete[](void* ptr) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): sized-delete overrides required by the C++ standard
void operator delete(void* ptr, std::size_t) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): sized-delete overrides required by the C++ standard
void operator delete[](void* ptr, std::size_t) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

#endif  // SCORE_MW_COM_EXAMPLES_HEAP_CHECK_HEAP_CHECK_H
