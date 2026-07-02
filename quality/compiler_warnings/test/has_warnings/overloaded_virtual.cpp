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

/// @file overloaded_virtual.cpp
/// @brief Triggers exactly one warning: -Woverloaded-virtual.
/// MISRA C++:2023 Rule 10.2.0 — overriding virtual functions.

#include <cstdint>
#include <tuple>

namespace compiler_warnings_test
{

struct VBase
{
    virtual void process(std::int32_t x)
    {
        std::ignore = x;
    }
    virtual ~VBase() = default;
};

struct VDerived : VBase
{
    void process(double x)
    {
        std::ignore = x;
    }  // warning: hides VBase::process(int)
};

}  // namespace compiler_warnings_test
