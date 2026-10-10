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
namespace impl
{

template <typename T>
class ContainerImpl
{
  public:
    /// \brief Add an element.
    void push(const T& value);

    /// \brief Get the last element.
    T pop();

    /// \brief Get the size.
    int size() const;

    /// \brief Check if empty.
    bool empty() const;

  private:
    T* data_;
    int size_;
};

}  // namespace impl

/// \brief A generic container (template type alias).
/// \api
template <typename T>
using Container = impl::ContainerImpl<T>;

}  // namespace test
