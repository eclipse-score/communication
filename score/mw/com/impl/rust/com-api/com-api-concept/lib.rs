/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

///This is main library file for the com-api-concept crate, which defines the core traits, types, and macros for the communication API concept.
/// It re-exports the main components of the crate, including the interface macro and the Reloc type, which is used for safe data relocation in the communication API.
/// The interface macro generates the necessary types and trait implementations for defining communication interfaces,
/// while the Reloc type provides a safe abstraction for moving data across thread or process boundaries without violating Rust's ownership rules.

pub use paste::paste;
mod com_api_concept;
mod interface_macros;
mod reloc;
pub use com_api_concept::*;
pub use reloc::Reloc;
