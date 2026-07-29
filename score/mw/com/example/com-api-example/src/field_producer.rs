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

#![allow(unused)]

use score_com::{Builder, FieldPublisher, InstanceSpecifier, Interface, Producer, Runtime};

use com_api_gen::{Exhaust, Tire, VehicleFieldInterface};

// VehicleFieldProducer is the producer type for the VehicleField interface (before offering)
type VehicleFieldProducer<R> = <VehicleFieldInterface as Interface>::Producer<R>;
// VehicleFieldOfferedProducer is the offered producer type for the VehicleField interface (fields support update/set-handler)
type VehicleFieldOfferedProducer<R> =
    <<VehicleFieldInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;

// Below function just demonstrate the field APIs usage
// This build fine but it can not run because we have not implemented the field APIs in Lola runtime yet.

// Producer creation and intialization of fields with initial values and set handlers for the fields
// It will return the offered producer instance which can be used to update the fields.
fn create_producer_field<R: Runtime + 'static>(
    runtime: &R,
    service_id: InstanceSpecifier,
    initial_tire_value: Tire,
    initial_exhaust_value: Exhaust,
) -> VehicleFieldOfferedProducer<R>
where
    <R as Runtime>::FieldPublisher<Tire>: Send + Sync,
    <R as Runtime>::FieldPublisher<Exhaust>: Send,
{
    let producer_builder = runtime.producer_builder::<VehicleFieldInterface>(service_id);
    let producer = producer_builder
        .build()
        .expect("Failed to build producer instance");

    // Use validator pattern with compile-time type-state validation
    // Must register handlers and initialize all fields before offer() is available
    let offered = producer
        .init_field()
        .register_set_handler_left_tire(move |val: &Tire| {
            println!("Received tire pressure update: {:?}", val);
            // Additional logic to handle the tire pressure update can be added here
            // For example, we can increment value or conver unit and update the field again.
            // TODO: in working example add that logic to demonstrate the set handler usage.
            // Note: I think producer may be need clone ?
        })
        .expect("Failed to register set handlers")
        .register_set_handler_exhaust(|_val: &Exhaust| {
            println!("Received exhaust update");
        })
        .expect("Failed to register set handlers")
        .update_left_tire(&initial_tire_value)
        .expect("Failed to update left_tire field")
        .update_exhaust(&initial_exhaust_value)
        .expect("Failed to update exhaust field")
        .offer()
        .expect("Failed to offer producer instance");

    offered
}

// Function to demonstrate the usage of the offered producer to update fields
fn offered_producer_process<R: Runtime>(offered_producer: VehicleFieldOfferedProducer<R>) {
    // Use the offered producer to update fields
    let new_tire_value = Tire { pressure: 32.0 };
    let new_exhaust_value = Exhaust {};
    offered_producer
        .left_tire
        .update(&new_tire_value)
        .expect("Failed to update left_tire field");
    offered_producer
        .exhaust
        .update(&new_exhaust_value)
        .expect("Failed to update exhaust field");
}
