// *******************************************************************************
// Copyright (c) 2026 Contributors to the Eclipse Foundation
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

#pragma once

namespace test
{

/// \brief A buffer with ref-qualified member overloads.
/// \api
struct Buffer
{
    /// \brief Access data on an lvalue.
    int* data() &;

    /// \brief Access data on an rvalue.
    int* data() &&;

    /// \brief Size, callable on const lvalue.
    int size() const&;
};

}  // namespace test
