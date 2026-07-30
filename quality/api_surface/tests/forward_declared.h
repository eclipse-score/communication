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

// Forward-declared implementation type (pimpl idiom): incomplete, defined
// elsewhere in a .cpp translation unit.
class WidgetImpl;

// A standalone incomplete/forward-declared type used only by reference.
struct IncompleteHandle;

/// \brief A widget using the pimpl idiom.
/// \api
class Widget
{
  public:
    /// \brief Construct a widget.
    Widget();

    /// \brief Destroy a widget.
    ~Widget();

    /// \brief Render the widget.
    void render();

  private:
    WidgetImpl* impl_;
};

}  // namespace test
