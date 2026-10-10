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

#include <string>

namespace test
{

/// \brief A class that exposes no public members at all.
///
/// Negative case: only the class type itself is part of the API surface; none
/// of its members (all private) may appear as public symbols.
/// \api
class OnlyPrivate
{
  private:
    int secret_;
    std::string token_;
    void mutate();
    int compute() const;
};

namespace detail
{

/// \brief A helper that lives in an internal namespace.
///
/// Negative case: nothing declared inside `test::detail` may be exported to the
/// public API surface, regardless of access specifier.
class HiddenHelper
{
  public:
    void publicButInternal();
    int alsoInternal(int x) const;
};

void hiddenFreeFunction();

}  // namespace detail

}  // namespace test
