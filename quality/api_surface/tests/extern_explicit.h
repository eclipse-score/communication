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

/// \brief A container template with explicit extern instantiations.
/// \api
template <typename T>
class Array
{
  public:
    /// \brief Append a value.
    void push(T value);

    /// \brief Access an element.
    T at(int index) const;

    /// \brief Number of elements.
    int size() const;
};

// Explicit extern template instantiations (declarations).
extern template class Array<int>;
extern template class Array<double>;

/// \brief An externally-defined configuration global.
/// \api
extern int g_globalConfig;

/// \brief An externally-defined application name pointer.
/// \api
extern const char* g_appName;

}  // namespace test
