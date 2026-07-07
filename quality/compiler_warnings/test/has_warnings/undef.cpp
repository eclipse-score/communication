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

/// @file undef.cpp
/// @brief Triggers warning: -Wundef.

#include <cstdint>

namespace compiler_warnings_test
{

#if UNDEFINED_MACRO
static const std::int32_t undef_guard = 1;
#endif

}  // namespace compiler_warnings_test
