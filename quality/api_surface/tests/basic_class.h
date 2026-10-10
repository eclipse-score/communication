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

/// \brief A basic calculator class.
/// \api
class Calculator
{
  public:
    /// \brief Default constructor.
    Calculator();

    /// \brief Destructor.
    ~Calculator();

    /// \brief Add two integers.
    int add(int a, int b);

    /// \brief Subtract two integers.
    int subtract(int a, int b);

    /// \brief Multiply two doubles.
    double multiply(double a, double b);

    /// \brief Reset the calculator.
    void reset();

  private:
    int result_;
    bool initialized_;
};

}  // namespace test
