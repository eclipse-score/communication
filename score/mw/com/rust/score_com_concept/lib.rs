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

/// This is main library file for the score_com_concept crate, which defines the core traits, types,
/// and macros for the communication API score_com_concept.
/// It re-exports the main components of the crate, including the interface macro and the Reloc
/// type, which is used for safe data relocation in the communication API. The crate is accessible
/// through the `score_com` crate.
/// The interface macro generates the necessary types and trait implementations for defining
/// communication interfaces,
/// while the Reloc type provides a safe abstraction for moving data across thread or process
/// boundaries without violating Rust's ownership rules.
mod concept;
mod error;
mod field_concept;
mod interface_macros;
mod reloc;
pub use concept::*;
pub use error::*;
pub use field_concept::*;
pub use interface_macros::{HandlerNotSet, HandlerSet, Init, Uninit};
#[doc(hidden)]
pub use paste;
pub use reloc::Reloc;
pub use score_com_macros;
