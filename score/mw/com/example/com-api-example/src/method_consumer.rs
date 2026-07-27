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

use com_api::{
    Builder, FindServiceSpecifier, InstanceSpecifier, Interface, MethodCaller,
    MethodInArgMaybeUninit, Runtime, ServiceDiscovery,
};

use com_api_gen::{Tire, VehicleMethodsInterface};

type VehicleMethodConsumer<R> = <VehicleMethodsInterface as Interface>::Consumer<R>;

// These functions are just to demonstrate the method APIs, and they can not be used in main of example app,
// as runtime implementation is not available for method APIs.
fn create_consumer_method<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleMethodConsumer<R> {
    let consumer_discovery =
        runtime.find_service::<VehicleMethodsInterface>(FindServiceSpecifier::Specific(service_id));
    let available_service_instances = consumer_discovery
        .get_available_instances()
        .expect("Failed to get available service instances");

    // Select service instance at specific handle_index
    let handle_index = 0; // or any index you need from vector of instances
    let consumer_builder = available_service_instances
        .into_iter()
        .nth(handle_index)
        .expect("Failed to get consumer builder at specified handle index");

    consumer_builder
        .build()
        .expect("Failed to build consumer instance")
}

// Method calls return `impl Future<Output = com_api::Result<T>>`, so they must be `.await`ed.
// We are having tuple of arguments, so we can have any number of arguments (currently up to 2) without any extra boilerplate.
// But in Method signature, we need to have tuple of arguments, so for zero argument method, we need to pass empty tuple.
// Which need to be improved using macro generated wrapper around method call, so that we can call zero argument method without empty tuple.
// even with argument tuple, method call can be improve using macro generated wrapper, so that we can call method with any number of arguments without tuple.

async fn consumer_method_processing<R: Runtime>(consumer: VehicleMethodConsumer<R>) {
    // Copy path: single positional argument — no tuple needed.
    let tire = Tire { pressure: 30.0 };
    match consumer.update_tire_pressure(tire).await {
        Ok(_) => println!("Successfully called update_tire_pressure method"),
        Err(e) => eprintln!("Failed to call update_tire_pressure method: {:?}", e),
    }

    let (uninit1,) = consumer
        .update_tire_pressure
        .allocate()
        .expect("Failed to allocate method arguments");
    let tire1ptr = uninit1.write(Tire { pressure: 35.0 });

    // Copy path: zero-argument method — empty parens, no empty-tuple needed.
    match consumer.get_tire_pressure().await {
        Ok(tire) => println!("Current tire pressure: {:?}", tire),
        Err(e) => eprintln!("Failed to call get_tire_pressure method: {:?}", e),
    }

    // Zero-copy path: allocate, write, then call the same wrapper.
    match consumer.update_tire_pressure(tire1ptr).await {
        Ok(_) => println!("Successfully called update_tire_pressure method with allocated args"),
        Err(e) => eprintln!(
            "Failed to call update_tire_pressure method with allocated args: {:?}",
            e
        ),
    }

    // Copy path: two arguments method.
    let tire1 = Tire { pressure: 31.0 };
    let tire2 = Tire { pressure: 32.0 };
    match consumer.update_front_tires_pressure(tire1, tire2).await {
        Ok(_) => println!("Successfully called update_front_tires_pressure method"),
        Err(e) => eprintln!("Failed to call update_front_tires_pressure method: {:?}", e),
    }

    let (uninit1, uninit2) = consumer
        .update_front_tires_pressure
        .allocate()
        .expect("Failed to allocate method arguments");
    let tire1ptr = uninit1.write(Tire { pressure: 36.0 });
    let tire2ptr = uninit2.write(Tire { pressure: 37.0 });
    // Zero-copy path: allocate, write both args, then call the same method with allocated args.
    match consumer
        .update_front_tires_pressure(tire1ptr, tire2ptr)
        .await
    {
        Ok(_) => {
            println!("Successfully called update_front_tires_pressure method with allocated args")
        }
        Err(e) => eprintln!(
            "Failed to call update_front_tires_pressure method with allocated args: {:?}",
            e
        ),
    }
}
