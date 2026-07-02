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

/// @file shadow.cpp
/// @brief Triggers exactly one warning: -Wshadow.
/// MISRA C++:2023 Rule 6.4.1 — identifier hiding.
/// CWE-398: Indicator of Poor Code Quality.

#include <cstdint>

namespace compiler_warnings_test
{

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

}  // namespace compiler_warnings_test
