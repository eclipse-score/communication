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

pub mod consumer;
pub mod producer;
pub mod vehicle_monitor;
pub use vehicle_monitor::{
    VehicleConsumer, VehicleMonitorConsumer, VehicleMonitorProducer, VehicleOfferedProducer,
};

/// All execution modes supported by the example application.
/// Used by clap for CLI argument parsing and by the consumer/producer to select behaviour.
#[derive(clap::ValueEnum, Clone, Debug)]
pub enum ExampleType {
    /// Synchronous service discovery and synchronous receive.
    Sync,
    /// Asynchronous service discovery, then synchronous receive.
    AsyncServiceDiscovery,
    /// Asynchronous service discovery and asynchronous receive (no timeout).
    AsyncReceive,
    /// Asynchronous service discovery and asynchronous receive with a cancellation timeout.
    AsyncReceiveWithTimeout,
    /// Asynchronous service discovery and stream-based receive.
    Streaming,
}
