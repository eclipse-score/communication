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

// This demo app writing and reading tire pressure data using producer and consumer respectively.
// It is demonstrating the composition of consumer and producer in one struct,
// but they can be used separately as well.
// The example is using Lola runtime, but it can be used with any runtime by changing the runtime initialization part.
// Note: The example is using unwrap and panic in some places for simplicity,
// but it is recommended to handle errors properly in production code.

#![allow(unused)]

use com_api::{Builder, InstanceSpecifier, Interface, Producer, Runtime};

use com_api_gen::{Tire, VehicleMethodsInterface};

type VehicleMethodOfferedProducer<R> =
    <<VehicleMethodsInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;

fn create_producer_method<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleMethodOfferedProducer<R> {
    let producer_builder = runtime.producer_builder::<VehicleMethodsInterface>(service_id);
    let producer = producer_builder
        .build()
        .expect("Failed to build producer instance");
    producer
        .init()
        .register_update_tire_pressure_handler(|tire: Tire| {
            println!("Received update_tire_pressure call with tire: {:?}", tire);
            ()
        })
        .register_get_tire_pressure_handler(|| {
            println!("Received get_tire_pressure call");
            // Return a sample tire pressure value, just dummy value returned for demonstration
            Tire { pressure: 32.0 }
        })
        .register_update_front_tires_pressure_handler(|tire1: Tire, tire2: Tire| {
            println!(
                "Received update_front_tires_pressure call with tire1: {:?}, tire2: {:?}",
                tire1, tire2
            );
            ()
        })
        .offer()
        .expect("Failed to offer producer instance")
}
