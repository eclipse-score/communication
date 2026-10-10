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

class Shape
{
  public:
    /// \brief Get the area.
    double area() const;

    /// \brief Get the perimeter.
    double perimeter() const;

  protected:
    void setDirty();

  private:
    double cached_area_;
};

class Circle : public Shape
{
  public:
    /// \brief Construct a circle with given radius.
    Circle(double radius);

    /// \brief Get the radius.
    double radius() const;

  private:
    double radius_;
};

}  // namespace impl

/// \brief A circle shape (exposed via type alias, inherits Shape methods).
/// \api
using Circle = impl::Circle;

}  // namespace test
