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

/// @file sign_compare.cpp
/// @brief Triggers exactly one warning: -Wsign-compare.
/// CWE-195: Signed to Unsigned Conversion Error.
/// CWE-697: Incorrect Comparison.

#include <cstddef>
#include <cstdint>

namespace compiler_warnings_test
{

bool signed_unsigned_compare(std::int32_t index, std::size_t size)
{
    return index < size;  // warning: signed/unsigned mismatch
}

}  // namespace compiler_warnings_test
