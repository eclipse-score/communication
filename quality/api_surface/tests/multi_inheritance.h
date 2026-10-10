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

class Drawable
{
  public:
    /// \brief Draw the object.
    void draw() const;
};

class Serializable
{
  public:
    /// \brief Serialize the object.
    void serialize() const;
};

class Loggable
{
  public:
    /// \brief Log a message (private base - not public API).
    void log() const;
};

class Trackable
{
  public:
    /// \brief Track the object (protected base - not public API).
    void track() const;
};

// Multiple inheritance: two public bases contribute to the API surface,
// a private base and a protected base do NOT.
class Widget : public Drawable, public Serializable, private Loggable, protected Trackable
{
  public:
    /// \brief Construct a widget.
    Widget();

    /// \brief Update the widget.
    void update();
};

}  // namespace impl

/// \brief A widget exposed via alias; inherits from multiple public bases.
/// \api
using Widget = impl::Widget;

}  // namespace test
