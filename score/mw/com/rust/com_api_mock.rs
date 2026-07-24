/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

//! Test-only re-export of the full COM API including the mock runtime.
//!
//! Depend on `//score/mw/com/rust:com_api_mock` (instead of `:com_api`) in your
//! test `BUILD` rule to get access to `MockRuntimeBuilderImpl` and `MockRuntimeImpl`.

pub use com_api::*;
pub use com_api_runtime_mock::MockRuntimeImpl;
pub use com_api_runtime_mock::RuntimeBuilderImpl as MockRuntimeBuilderImpl;
