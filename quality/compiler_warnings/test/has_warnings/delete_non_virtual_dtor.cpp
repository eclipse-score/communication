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

/// @file delete_non_virtual_dtor.cpp
/// @brief Triggers warning: -Wdelete-non-virtual-dtor.

namespace compiler_warnings_test
{

struct Base
{
    virtual void method() {}
    ~Base() {}
};

struct Derived : Base
{
    void method() override {}
};

void delete_polymorphic(Base* b)
{
    delete b;
}

}  // namespace compiler_warnings_test
