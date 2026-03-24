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

//! Integration tests for COM API with Lola runtime
//!
//! This crate contains comprehensive integration tests for the COM API when used with the Lola runtime implementation.
//! The tests cover various aspects of the COM API, including producer and consumer interactions, data transmission, and service discovery.
//! The tests are designed to validate the correct functioning of the COM API abstractions when interfacing with the Lola middleware,
//! ensuring that data is correctly produced, transmitted, and consumed across different scenarios.
//! The test suite includes cases for primitive data types as well as user-defined types.

pub mod test_types;

#[cfg(test)]
mod integration_test;
