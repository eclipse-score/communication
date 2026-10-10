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

/// \brief Status codes for operations.
/// \api
enum class Status
{
    kOk,
    kError,
    kTimeout,
    kCancelled
};

/// \brief Initialize the system.
/// \api
void Initialize();

/// \brief Shutdown the system.
/// \api
void Shutdown();

/// \brief Free function with complex signature.
/// \api
int ProcessData(const char* input, int length, Status* out_status);

}  // namespace test
