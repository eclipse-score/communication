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
namespace support
{

template <typename T>
class Result
{
  public:
    T value;
};

class Widget
{
  public:
    int id;
};

}  // namespace support
}  // namespace test

namespace leak
{

// Must not be extracted for the public API target.
class TransitiveLeak
{
  public:
    void should_not_be_public();
};

}  // namespace leak
