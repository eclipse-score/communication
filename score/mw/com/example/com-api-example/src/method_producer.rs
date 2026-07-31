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

// This file demonstrate the usage of producer method APIs, which are generated for the VehicleMethodsInterface.

// Notes: we are creating producer instance specific for method here but this is just for demonstration purpose,
// for same producer instance method / event/ field can be offered as per offer interface.

// All the functions and types in this file are just for demonstration purpose,
// as this are not part of any callable because of that unused warning is suppressed for this file.
use score_com::{Builder, InstanceSpecifier, Interface, Producer, Runtime};

use com_api_gen::{Tire, VehicleMethodsInterface};

#[allow(dead_code)]
type VehicleMethodOfferedProducer<R> =
    <<VehicleMethodsInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;

// These functions are just to demonstrate the method APIs, and they can not be used in main of example app,
// as runtime implementation is not available for method APIs.
// Once producer instance is created, it must be registered with method handlers.
// As this can be called concurrently from runtime, the user needs to handle the synchronization of data if required.
// The method handlers are registered using the `register_<method_name>_handler` methods on the producer instance.
// The handlers are registered before offering the producer instance, so that the consumer can call the methods on the producer instance.
// If user call `producer.offer()` before registering the handlers, it will panic, as handlers are not registered yet.
// User must call the `producer.init()` and register all method handlers,
// and if they forget to register one of the method handlers then the compiler will give an error,
// as the offer method will require all method handlers to be registered before offering the producer instance.
// Here assumption of use is user must call`init()` and register all method handlers ,
// direct call to `offer()` API will panic.
#[allow(dead_code)]
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
        // register method handler like function pointer.
        .register_update_tire_pressure_handler(process_left_tire)
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
        })
        .offer()
        .expect("Failed to offer producer instance")
}

#[allow(dead_code)]
fn process_left_tire(tire: Tire) {
    // do some processing with the tire data
    println!("Processing left tire pressure: {:?}", tire);
}
