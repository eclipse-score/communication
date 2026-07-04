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

class PointImpl
{
  public:
    double x() const;
    double y() const;
    double distance() const;

  private:
    double x_;
    double y_;
};

}  // namespace impl

/// \brief A point in 2D space (type alias to impl).
/// \api
using Point = impl::PointImpl;

}  // namespace test
