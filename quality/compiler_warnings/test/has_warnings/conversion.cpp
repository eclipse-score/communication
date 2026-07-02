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

/// @file conversion.cpp
/// @brief Triggers exactly one warning: -Wconversion / -Wfloat-conversion.
/// MISRA C++:2023 Rule 7.0.5 — integral/floating conversion.
/// CWE-681: Incorrect Conversion between Numeric Types.

#include <cstdint>

namespace compiler_warnings_test
{

std::int32_t implicit_narrowing(double input)
{
    std::int32_t result = input;  // warning: implicit lossy conversion
    return result;
}

}  // namespace compiler_warnings_test
