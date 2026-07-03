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
/// @brief False-positive baseline: should compile cleanly with warning features enabled.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <tuple>

namespace compiler_warnings_test
{

// strict_warnings: clean patterns

std::int32_t truncate_to_int(double value)
{
    return static_cast<std::int32_t>(value);
}

bool is_valid_index(std::int32_t index, std::size_t size)
{
    if (index < 0)
    {
        return false;
    }
    return static_cast<std::size_t>(index) < size;
}

std::int32_t callback_handler(std::int32_t event_id, std::int32_t context, std::int32_t reserved)
{
    std::ignore = context;
    std::ignore = reserved;
    return event_id + 1;
}

std::int32_t modern_handler([[maybe_unused]] std::int32_t event_id, [[maybe_unused]] std::int32_t flags)
{
    return 0;
}

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

// additional_warnings: clean patterns

std::int32_t use_const_safely(const std::int32_t* ptr)
{
    std::int32_t copy = *ptr;
    copy += 1;
    return copy;
}

void safe_format_print(std::int32_t value)
{
    std::printf("value = %d\n", value);
}

#define FEATURE_ENABLED 1
#if FEATURE_ENABLED
static const std::int32_t feature_constant = 42;
#endif

void aligned_copy(const void* src, std::int32_t* dst)
{
    const auto* typed_src = static_cast<const std::int32_t*>(src);
    *dst = *typed_src;
}

// strict_warnings (C++ specific): clean patterns

struct SafeBase
{
    virtual void method() {}
    virtual ~SafeBase() = default;
};

struct SafeDerived : SafeBase
{
    void method() override {}
};

struct ProcessBase
{
    virtual void process(std::int32_t x)
    {
        std::ignore = x;
    }
    virtual ~ProcessBase() = default;
};

struct ProcessDerived : ProcessBase
{
    using ProcessBase::process;  // brings base version into scope
    void process(double x)
    {
        std::ignore = x;
    }
};

std::size_t safe_subtract(std::size_t a, std::size_t b)
{
    return (a > b) ? (a - b) : 0U;
}

inline std::int32_t get_feature_constant()
{
    return feature_constant;
}

}  // namespace compiler_warnings_test
