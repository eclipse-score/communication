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

//! This crates implements the COM API runtime for Lola
//! It provides the necessary structures and implementations to interact with the Lola communication middleware
//! through the COM API abstractions defined in the `com_api_concept` crate.
//! It includes runtime implementation, producer and consumer builders, and other related components.
//! It leverages FFI bindings to interface with the underlying Lola middleware.
//! It has three main modules:
//! - `runtime`: Contains the runtime implementation for Lola.
//! - `producer`: Contains the producer builder and related structures for Lola.
//! - `consumer`: Contains the consumer discovery and subscriber implementations for Lola.
//! Each module is designed to encapsulate specific functionalities, promoting modularity and maintainability.
//! The crate is structured to facilitate easy integration and usage of the Lola middleware within applications
//! that utilize the COM API abstractions.

mod consumer;
mod producer;
mod runtime;

pub use consumer::{LolaConsumerInfo, Sample, SampleConsumerDiscovery, SubscribableImpl};
pub use producer::{
    LolaProviderInfo, Publisher, SampleMaybeUninit, SampleMut, SampleProducerBuilder,
};
pub use runtime::{LolaRuntimeImpl, RuntimeBuilderImpl};
