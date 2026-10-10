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

/// @file cast_qual.cpp
/// @brief Triggers warning: -Wcast-qual.

#include <cstdint>

namespace compiler_warnings_test
{

void cast_away_const(const std::int32_t* ptr)
{
    std::int32_t* mutable_ptr = (std::int32_t*)ptr;
    *mutable_ptr = 42;
}

}  // namespace compiler_warnings_test
