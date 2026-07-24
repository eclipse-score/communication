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

#include "quality/api_surface/tests/result_type_support.h"

namespace test
{

/// \brief Factory exposing result-typed API signatures.
/// \api
class ResultFactory
{
  public:
    /// \brief Create a widget.
    static support::Result<support::Widget> CreateWidget();
};

}  // namespace test
