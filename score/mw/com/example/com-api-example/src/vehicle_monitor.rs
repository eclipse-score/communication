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

use com_api::{Interface, Producer, Runtime, Subscriber};
use com_api_gen::{Exhaust, Tire, VehicleInterface};

// Type aliases for generated consumer and offered producer types for the Vehicle interface
// VehicleConsumer is the consumer type generated for the Vehicle interface, parameterized by the runtime R
pub type VehicleConsumer<R> = <VehicleInterface as Interface>::Consumer<R>;
// VehicleOfferedProducer is the offered producer type generated for the Vehicle interface, parameterized by the runtime R
pub type VehicleOfferedProducer<R> =
    <<VehicleInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;

// Example struct demonstrating composition with VehicleConsumer
pub struct VehicleMonitorProducer<R: Runtime> {
    pub producer: VehicleOfferedProducer<R>,
}

pub struct VehicleMonitorConsumer<R: Runtime> {
    pub tire_subscriber: <<R as Runtime>::Subscriber<Tire> as Subscriber<Tire, R>>::Subscription,
    pub _exhaust_subscriber:
        <<R as Runtime>::Subscriber<Exhaust> as Subscriber<Exhaust, R>>::Subscription,
}
