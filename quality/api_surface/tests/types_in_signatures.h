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

/// \brief A 2D point used in API signatures.
/// \api
struct Point
{
    double x;
    double y;

    /// \brief Constructor.
    Point(double x, double y);

    /// \brief Distance from origin.
    double distance() const;
};

/// \brief A line segment defined by two points.
/// \api
struct Line
{
    Point p1;
    Point p2;

    /// \brief Constructor.
    Line(const Point& p1, const Point& p2);

    /// \brief Get the length.
    double length() const;
};

/// \brief Geometry operations using the types above.
/// \api
class GeometryCalculator
{
  public:
    /// \brief Calculate midpoint.
    Point calculateMidpoint(const Point& p1, const Point& p2);

    /// \brief Calculate distance between points.
    double calculateDistance(const Point& p1, const Point& p2);

    /// \brief Calculate minimum distance between two lines.
    double minLineDistance(const Line& l1, const Line& l2);

  private:
    Point origin_{0.0, 0.0};
};

}  // namespace test
