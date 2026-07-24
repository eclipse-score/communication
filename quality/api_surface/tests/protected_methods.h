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

/// \brief A base class exposing protected extension points.
/// \api
class Base
{
  public:
    /// \brief Construct the base.
    Base();

    /// \brief A public operation.
    void publicMethod();

  protected:
    /// \brief A protected hook for subclasses (part of the API surface).
    void onEvent();

    /// \brief A protected helper for subclasses.
    int computeInternal(int x) const;

  private:
    void secret();
    int state_;
};

}  // namespace test
