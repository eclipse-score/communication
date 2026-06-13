// *******************************************************************************
// Copyright (c) 2025 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

/// @file clean.cpp
/// @brief False-positive baseline: Code that MUST compile cleanly with all
///        warning features enabled (-Wall -Wextra -Wshadow -Wconversion
///        -Wcast-align -Wcast-qual -Wformat-nonliteral -Wundef etc.).
///        Each function demonstrates a correct coding pattern that should NOT
///        be flagged by any warning.

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace compiler_warnings_test
{

// ---------------------------------------------------------------------------
// strict_warnings: correct patterns
// ---------------------------------------------------------------------------

// Explicit cast suppresses -Wconversion (developer intent is clear).
// MISRA C++:2023 Rule 7.0.5 — narrowing is acceptable when explicit.
std::int32_t truncate_to_int(double value)
{
    return static_cast<std::int32_t>(value);
}

// Guarded signed/unsigned comparison avoids -Wsign-compare.
// CWE-195: proper boundary check prevents signed-to-unsigned error.
bool is_valid_index(std::int32_t index, std::size_t size)
{
    if (index < 0)
    {
        return false;
    }
    return static_cast<std::size_t>(index) < size;
}

// (void) cast suppresses -Wunused-parameter for callback signatures.
// MISRA C++:2023 Rule 0.1.1 — parameter intentionally unused.
std::int32_t callback_handler(std::int32_t event_id, std::int32_t context, std::int32_t reserved)
{
    (void)context;
    (void)reserved;
    return event_id + 1;
}

// [[maybe_unused]] attribute — modern C++ suppression of -Wunused-parameter.
std::int32_t modern_handler([[maybe_unused]] std::int32_t event_id, [[maybe_unused]] std::int32_t flags)
{
    return 0;
}

// No shadowing: distinct variable names in nested scopes.
// MISRA C++:2023 Rule 6.4.1 — identifiers are not hidden.
std::int32_t no_shadowing(std::int32_t value)
{
    std::int32_t outer = value + 1;
    if (outer > 0)
    {
        std::int32_t inner = value * 2;
        return inner;
    }
    return outer;
}

// ---------------------------------------------------------------------------
// additional_warnings: correct patterns
// ---------------------------------------------------------------------------

// const_cast alternative: work on a copy instead of casting away const.
// Avoids -Wcast-qual.
std::int32_t use_const_safely(const std::int32_t* ptr)
{
    std::int32_t copy = *ptr;
    copy += 1;
    return copy;
}

// Literal format strings avoid -Wformat-nonliteral.
void safe_format_print(std::int32_t value)
{
    std::printf("value = %d\n", value);
}

// Defined macro avoids -Wundef in #if directives.
#define FEATURE_ENABLED 1
#if FEATURE_ENABLED
static const std::int32_t feature_constant = 42;
#endif

// Aligned pointer cast avoids -Wcast-align.
void aligned_copy(const void* src, std::int32_t* dst)
{
    const auto* typed_src = static_cast<const std::int32_t*>(src);
    *dst = *typed_src;
}

// ---------------------------------------------------------------------------
// strict_warnings (C++ specific): correct patterns
// ---------------------------------------------------------------------------

// Virtual destructor in polymorphic base avoids -Wdelete-non-virtual-dtor.
// MISRA C++:2023 Rule 15.7.1.
struct SafeBase
{
    virtual void method() {}
    virtual ~SafeBase() = default;
};

struct SafeDerived : SafeBase
{
    void method() override {}
};

// using-declaration avoids -Woverloaded-virtual when adding overloads.
struct ProcessBase
{
    virtual void process(std::int32_t x)
    {
        (void)x;
    }
    virtual ~ProcessBase() = default;
};

struct ProcessDerived : ProcessBase
{
    using ProcessBase::process;  // brings base version into scope
    void process(double x)
    {
        (void)x;
    }
};

// Safe arithmetic with no implicit conversions.
std::size_t safe_subtract(std::size_t a, std::size_t b)
{
    return (a > b) ? (a - b) : 0U;
}

// Suppress unused-variable warning for feature_constant.
inline std::int32_t get_feature_constant()
{
    return feature_constant;
}

// ---------------------------------------------------------------------------
// Clang -Wthread-safety: correct pattern (Clang-only, GCC ignores attributes)
// Ref: https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
// ---------------------------------------------------------------------------

#ifdef __clang__
class __attribute__((capability("mutex"))) AnnotatedMutex
{
  public:
    void lock() __attribute__((acquire_capability())) {}
    void unlock() __attribute__((release_capability())) {}
};

class SafeGuardedCounter
{
    AnnotatedMutex mu_;
    int count_ __attribute__((guarded_by(mu_))) = 0;

  public:
    void increment() __attribute__((requires_capability(mu_)))
    {
        count_++;  // safe: lock is required by contract
    }
};
#endif

}  // namespace compiler_warnings_test
