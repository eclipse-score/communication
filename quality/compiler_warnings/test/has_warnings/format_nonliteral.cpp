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

/// @file format_nonliteral.cpp
/// @brief Triggers exactly one warning: -Wformat-nonliteral.
/// CWE-134: Uncontrolled Format String.

#include <cstdint>
#include <cstdio>

namespace compiler_warnings_test
{

void format_nonliteral(const char* fmt, std::int32_t value)
{
    std::printf(fmt, value);  // warning: format string is not a literal
}

}  // namespace compiler_warnings_test
