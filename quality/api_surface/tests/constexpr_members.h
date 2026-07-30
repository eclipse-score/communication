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

/// \brief A 2D vector with constexpr members.
/// \api
class Vector2
{
  public:
    /// \brief Constexpr constructor.
    constexpr Vector2(double x, double y);

    /// \brief Constexpr accessor.
    constexpr double x() const;

    /// \brief Constexpr accessor.
    constexpr double y() const;

    /// \brief A static constexpr data member.
    static constexpr double kUnit = 1.0;
};

/// \brief A constexpr free function.
/// \api
constexpr double Dot(const Vector2& a, const Vector2& b);

}  // namespace test
