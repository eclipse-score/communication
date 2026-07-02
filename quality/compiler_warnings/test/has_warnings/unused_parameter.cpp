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

/// @file unused_parameter.cpp
/// @brief Triggers exactly one warning: -Wunused-parameter.
/// MISRA C++:2023 Rule 0.1.1 — unreachable/dead code.
/// CWE-561: Dead Code.

#include <cstdint>

namespace compiler_warnings_test
{

std::int32_t unused_parameter(std::int32_t input, std::int32_t unused_param)
{
    return input * 2;  // warning: 'unused_param' never used
}

}  // namespace compiler_warnings_test
