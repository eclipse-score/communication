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

// Include this file in exactly ONE translation unit per binary (the translation unit that defines main()).
//
// Overrides ALL replaceable global allocation functions (throwing, nothrow, array,
// and aligned forms) so no standard heap path bypasses the check.
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

// --- C++17 aligned forms ---

// NOLINTNEXTLINE(misc-new-delete-overloads): aligned operator new, C++17 replaceable global
void* operator new(std::size_t sz, std::align_val_t al)
{
    if (heap_check::tl_heap_forbidden)
    {
        std::abort();
    }
    // aligned_alloc requires size to be a multiple of alignment; round up.
    std::size_t align = static_cast<std::size_t>(al);
    std::size_t rounded = (sz + align - 1U) & ~(align - 1U);
    void* ptr = std::aligned_alloc(align, rounded);  // NOLINT(cppcoreguidelines-no-malloc)
    if (ptr == nullptr)
    {
        throw std::bad_alloc{};
    }
    return ptr;
}

// NOLINTNEXTLINE(misc-new-delete-overloads): aligned array operator new, C++17 replaceable global
void* operator new[](std::size_t sz, std::align_val_t al)
{
    return ::operator new(sz, al);
}

// NOLINTNEXTLINE(misc-new-delete-overloads): aligned operator delete, C++17 replaceable global
void operator delete(void* ptr, std::align_val_t) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): aligned array operator delete, C++17 replaceable global
void operator delete[](void* ptr, std::align_val_t) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): sized aligned operator delete, C++17 replaceable global
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): sized aligned array operator delete, C++17 replaceable global
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// --- Nothrow forms ---

// NOLINTNEXTLINE(misc-new-delete-overloads): nothrow operator new, replaceable global
void* operator new(std::size_t sz, const std::nothrow_t&) noexcept
{
    if (heap_check::tl_heap_forbidden)
    {
        std::abort();
    }
    return std::malloc(sz);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): nothrow array operator new, replaceable global
void* operator new[](std::size_t sz, const std::nothrow_t& nt) noexcept
{
    return ::operator new(sz, nt);
}

// NOLINTNEXTLINE(misc-new-delete-overloads): aligned nothrow operator new, C++17 replaceable global
void* operator new(std::size_t sz, std::align_val_t al, const std::nothrow_t&) noexcept
{
    if (heap_check::tl_heap_forbidden)
    {
        std::abort();
    }
    std::size_t align = static_cast<std::size_t>(al);
    std::size_t rounded = (sz + align - 1U) & ~(align - 1U);
    return std::aligned_alloc(align, rounded);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): aligned nothrow array operator new, C++17 replaceable global
void* operator new[](std::size_t sz, std::align_val_t al, const std::nothrow_t& nt) noexcept
{
    return ::operator new(sz, al, nt);
}

// NOLINTNEXTLINE(misc-new-delete-overloads): nothrow operator delete (placement delete for nothrow new)
void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): nothrow array operator delete (placement delete for nothrow new)
void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): aligned nothrow operator delete, C++17 replaceable global
void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

// NOLINTNEXTLINE(misc-new-delete-overloads): aligned nothrow array operator delete, C++17 replaceable global
void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept
{
    std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
}

#endif  // SCORE_MW_COM_EXAMPLES_HEAP_CHECK_HEAP_CHECK_H
