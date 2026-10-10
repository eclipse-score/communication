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

/// \brief A function whose result must not be discarded.
/// \api
[[nodiscard]] int Compute(int x);

/// \brief A deprecated function.
/// \api
[[deprecated("use Compute")]] int OldCompute(int x);

/// \brief A function that is both nodiscard and deprecated.
/// \api
[[nodiscard]] [[deprecated]] int LegacyCompute();

/// \brief A handle with attribute-annotated methods.
/// \api
class Handle
{
  public:
    /// \brief Whether the handle is valid.
    [[nodiscard]] bool valid() const;

    /// \brief Reset the handle (deprecated).
    [[deprecated]] void reset();
};

}  // namespace test
