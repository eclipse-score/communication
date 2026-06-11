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

/// @file has_warnings.cpp
/// @brief True-positive baseline: Code with genuine issues that MUST be caught
///        by the compiler warning features. Each function triggers exactly one
///        specific warning category.
///
/// Expected behavior:
///   Features disabled: compiles without error (tp_build_test validates this)
///   Features enabled:  triggers warnings / errors (future test ticket)

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace compiler_warnings_test
{

// ---------------------------------------------------------------------------
// strict_warnings coverage
// ---------------------------------------------------------------------------

// -Wconversion / -Wfloat-conversion: Implicit narrowing double -> int.
// MISRA C++:2023 Rule 7.0.5 — integral/floating conversion.
// CWE-681: Incorrect Conversion between Numeric Types.
std::int32_t implicit_narrowing(double input)
{
    std::int32_t result = input;  // warning: implicit lossy conversion
    return result;
}

// -Wshadow: Inner variable shadows outer variable.
// MISRA C++:2023 Rule 6.4.1 — identifier hiding.
// CWE-398: Indicator of Poor Code Quality.
std::int32_t variable_shadowing(std::int32_t value)
{
    std::int32_t result = value + 1;
    if (result > 0)
    {
        std::int32_t result = value * 2;  // warning: shadows outer 'result'
        return result;
    }
    return result;
}

// -Wsign-compare: Comparison between signed and unsigned.
// CWE-195: Signed to Unsigned Conversion Error.
// CWE-697: Incorrect Comparison.
bool signed_unsigned_compare(std::int32_t index, std::size_t size)
{
    return index < size;  // warning: signed/unsigned mismatch
}

// -Wunused-parameter: Parameter declared but never referenced.
// MISRA C++:2023 Rule 0.1.1 — unreachable/dead code.
// CWE-561: Dead Code.
std::int32_t unused_parameter(std::int32_t input, std::int32_t unused_param)
{
    return input * 2;  // warning: 'unused_param' never used
}

// ---------------------------------------------------------------------------
// additional_warnings coverage
// ---------------------------------------------------------------------------

// -Wcast-qual: Cast removes const qualifier.
// MISRA C++:2023 Rule 8.2.3 — const shall not be cast away.
void cast_away_const(const std::int32_t* ptr)
{
    std::int32_t* mutable_ptr = (std::int32_t*)ptr;  // warning: cast discards const
    *mutable_ptr = 42;
}

// -Wformat-nonliteral: Non-literal format string.
// CWE-134: Uncontrolled Format String.
void format_nonliteral(const char* fmt, int value)
{
    std::printf(fmt, value);  // warning: format string is not a literal
}

// -Wundef: Undefined macro used in #if.
// MISRA C:2012 Rule 20.9 / MISRA C++:2023 Dir 4.4.
#if UNDEFINED_MACRO  // warning: "UNDEFINED_MACRO" is not defined
static const int undef_guard = 1;
#endif

// ---------------------------------------------------------------------------
// strict_warnings (C++ specific) coverage
// ---------------------------------------------------------------------------

// -Wdelete-non-virtual-dtor: Deleting polymorphic object without virtual dtor.
// MISRA C++:2023 Rule 15.7.1 — polymorphic base needs virtual destructor.
struct Base
{
    virtual void method() {}
    ~Base() {}  // warning: non-virtual destructor in polymorphic class
};

struct Derived : Base
{
    void method() override {}
};

void delete_polymorphic(Base* b)
{
    delete b;  // warning: deleting object of polymorphic type with non-virtual dtor
}

// -Woverloaded-virtual: Derived function hides base virtual (different signature).
// MISRA C++:2023 Rule 10.2.0 — overriding virtual functions.
struct VBase
{
    virtual void process(std::int32_t x)
    {
        (void)x;
    }
    virtual ~VBase() = default;
};

struct VDerived : VBase
{
    void process(double x)
    {
        (void)x;
    }  // warning: hides VBase::process(int)
};

}  // namespace compiler_warnings_test
