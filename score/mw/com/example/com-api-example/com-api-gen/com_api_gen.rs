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

use com_api::{interface, CommData, FieldPublisher, ProviderInfo, Publisher, Reloc, Subscriber};

#[derive(Debug, Reloc, CommData)]
#[repr(C)]
#[comm_data(id = "Tire")]
pub struct Tire {
    pub pressure: f32,
}

impl Default for Tire {
    fn default() -> Self {
        Tire { pressure: 0.0 }
    }
}

#[derive(Debug, Reloc, CommData)]
#[repr(C)]
// No explicit ID provided, so it will be auto-generated as "com_api_gen::Exhaust"
pub struct Exhaust {}

impl Default for Exhaust {
    fn default() -> Self {
        Exhaust {}
    }
}

// Example interface definition using the interface macro with a custom UID for the interface.
// This will generate the following types and trait implementations:
// - VehicleInterface struct with INTERFACE_ID = "VehicleInterface"
// - VehicleConsumer<R>, VehicleProducer<R>, VehicleOfferedProducer<R> with appropriate trait
//   implementations for the Vehicle interface.
// The macro invocation defines an interface named "Vehicle" with two events: "left_tire" and "exhaust".
// The generated code will include:
// - VehicleInterface struct with INTERFACE_ID = "VehicleInterface"
// - VehicleConsumer<R> struct that implements Consumer trait for subscribing to "left_tire"
//   and "exhaust" events.
// - VehicleProducer<R> struct that implements Producer trait for producing "left_tire" and
//   "exhaust" events.
// - VehicleOfferedProducer<R> struct that implements OfferedProducer trait for offering
//   "left_tire" and "exhaust" events.
interface!(
    interface Vehicle, {
        Id = "VehicleInterface",
        left_tire: Event<Tire>,
        exhaust: Event<Exhaust>,
     }
);

// Field intialize the intial value based on the default value of the field type at the time of creating field publisher.
// If user want to update the value after that as well then they can use update method of the field publisher to update the value of the field.
interface!(
    interface VehicleField, {
        Id = "VehicleFieldInterface",
        left_tire: Field<Tire>,
        exhaust: Field<Exhaust>,
     }
);
