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

use com_api::{interface, CommData, ProviderInfo, Publisher, Reloc, Subscriber};

#[derive(Debug, Reloc, CommData)]
#[repr(C)]
#[comm_data(id = "Tire")]
pub struct Tire {
    pub pressure: f32,
}

#[derive(Debug, Reloc, CommData)]
#[repr(C)]
// No explicit ID provided, so it will be auto-generated as "com_api_gen::Exhaust"
pub struct Exhaust {}

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
    interface Vehicle {
        Id = "VehicleInterface",
        left_tire: Event<Tire>,
        exhaust: Event<Exhaust>,
     }
);

// Example interface definition using the interface macro with a custom UID for the interface.
// First tuple is the input argument type, and the second tuple is the return type.
// This will generate the following types and trait implementations:
// - VehicleMethodsInterface struct with INTERFACE_ID = "VehicleMethodsInterface"
// - VehicleMethodsConsumer<R>, VehicleMethodsProducer<R>, VehicleMethodsOfferedProducer<R>
//   with appropriate trait implementations for the VehicleMethods interface.
// As passed methods to macro it will generate the following methods:
// - update_tire_pressure((Tire,)) -> ()
// - update_front_tires_pressure((Tire, Tire)) -> ()
// - get_tire_pressure(()) -> Tire
// and this method can be accessed through the consumer instance of VehicleMethodsConsumer<R>.
// Note: Yes currently macro is taking tuple based input and which makes littel bit not user-friendly,
// but this can be improved in future by generating wrapper methods.
// TODO: This interface macro need to be improved to support the mix of methods, events and fields in same interface.
// Also for method this need to take the input and output types in more user-friendly way, instead of tuple based input and output.
// example:  update_front_tires_pressure: Method<Input{Tire, Tire}, Output{Tire}>,
// Currently it is taking tuple based input and output, which is fine for now as this is just for design APIs build/usage verification.
interface!(
    interface VehicleMethods {
        Id = "VehicleMethodsInterface",
        update_tire_pressure: Method<(Tire,), ()>,
        update_front_tires_pressure: Method<(Tire,Tire), ()>,
        get_tire_pressure: Method<(), Tire>,
     }
);
