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

/// \brief A global tunable (constexpr).
/// \api
constexpr int kMaxItems = 100;

/// \brief A mutable global variable.
/// \api
int g_counter;

/// \brief An externally-defined global variable.
/// \api
extern int g_external;

/// \brief Return the library version string.
/// \api
const char* GetVersion();

/// \brief Enable or disable verbose logging.
/// \api
void SetVerbose(bool enabled);

}  // namespace test
