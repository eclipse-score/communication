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
pub use consumer::VehicleMonitorConsumer;
pub use producer::VehicleMonitorProducer;

use com_api::{Interface, Producer};
use com_api_gen::VehicleInterface;

// Type aliases for generated consumer and offered producer types for the Vehicle interface
// VehicleConsumer is the consumer type generated for the Vehicle interface, parameterized by the runtime R
pub type VehicleConsumer<R> = <VehicleInterface as Interface>::Consumer<R>;
// VehicleOfferedProducer is the offered producer type generated for the Vehicle interface, parameterized by the runtime R
pub type VehicleOfferedProducer<R> =
    <<VehicleInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;
