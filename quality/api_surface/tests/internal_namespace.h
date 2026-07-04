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
namespace detail
{

// This is in ::detail:: and should be filtered out of the public API
class InternalHelper
{
  public:
    void doStuff();
    int compute(int x);
};

}  // namespace detail

namespace internal
{

// This is in ::internal:: and should also be filtered out
class AnotherInternal
{
  public:
    void internalMethod();
};

}  // namespace internal

/// \brief Public API class.
/// \api
class PublicService
{
  public:
    /// \brief Start the service.
    void start();

    /// \brief Stop the service.
    void stop();

  private:
    detail::InternalHelper helper_;
};

}  // namespace test
