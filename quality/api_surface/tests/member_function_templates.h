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

/// \brief A registry with member function templates.
/// \api
class Registry
{
  public:
    /// \brief Add an item of any type.
    template <typename T>
    void add(const T& item);

    /// \brief Get an item of a requested type.
    template <typename T>
    T get(int index) const;

    /// \brief Clear the registry (non-template member).
    void clear();
};

}  // namespace test
