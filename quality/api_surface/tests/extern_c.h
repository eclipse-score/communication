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

extern "C" {

/// \brief Initialize the C-compatible subsystem.
/// \api
void c_init(int flags);

/// \brief Process a C-compatible data buffer.
/// \api
int c_process(const char* data, int len);

}  // extern "C"

/// \brief A standalone extern "C" function.
/// \api
extern "C" void c_shutdown();

}  // namespace test
