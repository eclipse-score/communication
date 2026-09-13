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

// This file demonstrate the usage of consumer method APIs, which are generated for the VehicleMethodsInterface.
// It shows how method can be called using copy and zero-copy arguments,
// And async method call can be awaited to get the result.

// Notes: we are creating consumer instance specific for method here but this is just for demonstration purpose,
// for same consumer instance method / event/ field can be consume as per offer interface.
// This can not be used or called in main of example app, as runtime implementation is not available for method APIs.

// All the functions and types in this file are just for demonstration purpose,
// as this are not part of any callable because of that unused warning is suppressed for this file.
use score_com::{
    Builder, FindServiceSpecifier, InstanceSpecifier, Interface, MethodCaller,
    MethodInArgMaybeUninit, Runtime, ServiceDiscovery,
};

use com_api_gen::{Tire, VehicleMethodsInterface};

#[allow(dead_code)]
type VehicleMethodConsumer<R> = <VehicleMethodsInterface as Interface>::Consumer<R>;

// These functions are just to demonstrate the method APIs, and they can not be used in main of example app,
// as runtime implementation is not available for method APIs.
#[allow(dead_code)]
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

// Method calls return `impl Future<Output = score_com::Result<T>>`, so they must be `.await`ed.
// All the method called is async.

// two arguments method.
// It demonstrates calling a method with two arguments, where the arguments are copied into the method call.
// It also demonstrates the zero-copy path, where the arguments are allocated, written, and then passed to the method call.
#[allow(dead_code)]
async fn consumer_processing<R: Runtime>(consumer: VehicleMethodConsumer<R>) {
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

// A method that is not just a thin wrapper around a field's get/set: it takes two Tire
// readings and returns a distinct computed result type (PressureImbalance), demonstrating both
// the copy path and the zero-copy path.
#[allow(dead_code)]
async fn consumer_calculate_pressure_imbalance<R: Runtime>(consumer: VehicleMethodConsumer<R>) {
    // Copy path: two arguments, copied into the method call.
    let tire1 = Tire { pressure: 30.0 };
    let tire2 = Tire { pressure: 32.0 };
    match consumer.calculate_pressure_imbalance(tire1, tire2).await {
        Ok(imbalance) => println!("Calculated pressure imbalance: {:?}", *imbalance),
        Err(e) => eprintln!("Failed to call calculate_pressure_imbalance method: {:?}", e),
    }

    // Zero-copy path: allocate, write both args, then call the same method with allocated args.
    let (uninit1, uninit2) = consumer
        .calculate_pressure_imbalance
        .allocate()
        .expect("Failed to allocate method arguments");
    let tire1ptr = uninit1.write(Tire { pressure: 33.0 });
    let tire2ptr = uninit2.write(Tire { pressure: 34.0 });
    match consumer
        .calculate_pressure_imbalance(tire1ptr, tire2ptr)
        .await
    {
        Ok(imbalance) => {
            println!(
                "Calculated pressure imbalance with allocated args: {:?}",
                *imbalance
            )
        }
        Err(e) => eprintln!(
            "Failed to call calculate_pressure_imbalance method with allocated args: {:?}",
            e
        ),
    }
}
