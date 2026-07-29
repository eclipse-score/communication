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

use score_com::{
    Builder, FieldSubscriber, FieldSubscription, FindServiceSpecifier, InstanceSpecifier,
    Interface, Runtime, SampleContainer, ServiceDiscovery, Subscriber, Subscription,
};

use com_api_gen::{Tire, VehicleFieldInterface};

type VehicleFieldConsumer<R> = <VehicleFieldInterface as Interface>::Consumer<R>;

// create the consumer.
fn create_consumer_field<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleFieldConsumer<R> {
    let consumer_discovery =
        runtime.find_service::<VehicleFieldInterface>(FindServiceSpecifier::Specific(service_id));
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

async fn process_get_method_async<S, R>(subscription: S)
where
    S: FieldSubscription<Tire, R>,
    R: Runtime,
{
    // Get field value asynchronously
    match subscription.get().await {
        Ok(_method_return) => {
            println!("Current tire pressure from spawned task");
        }
        Err(e) => eprintln!("Failed to get tire pressure: {:?}", e),
    }

    println!("Async subscription processing in spawned task completed");
}

// Function to demonstrate the usage of the consumer to get and set fields,
// Subscribe to the fields event and it provides the set and get method as well.
fn consumer_processing_field<R: Runtime + 'static>(consumer: VehicleFieldConsumer<R>)
where
    <<R as Runtime>::FieldSubscriber<Tire> as Subscriber<Tire, R>>::Subscription: Send + 'static,
{
    // Field consumer API methods
    // But they demonstrate the correct API usage pattern
    // TODO: Currently we are not offering the get method async in FieldSubscriber
    // because async call will may run in different thread and that will cause the issue in subscription.
    let _ = consumer
        .left_tire
        .get()
        .map(|result| println!("Got field value via consumer: {:?}", result));

    let _ = consumer
        .left_tire
        .set(&Tire { pressure: 30.0 })
        .map(|result| println!("Set field value via consumer: {:?}", result));

    // Subscribe to the field to receive updates
    let subscription = consumer
        .left_tire
        .subscribe(3)
        .expect("Failed to subscribe to field");

    // Create scope for sample_buf to ensure it's dropped before tokio::spawn
    {
        let mut sample_buf = SampleContainer::new(3);

        // Poll for updates (non-blocking)
        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(n) if n > 0 => {
                while let Some(sample) = sample_buf.pop_front() {
                    println!("Updated tire pressure: {:?}", *sample);
                }
            }
            _ => {
                println!("No new tire pressure updates available");
            }
        }
        // sample_buf is dropped here at end of scope
    }

    // Set via subscription
    let _ = subscription
        .set(&Tire { pressure: 35.0 })
        .map(|result| println!("Set field value via subscription: {:?}", result));

    // Spawn async task with subscription
    // The subscription is moved into the task
    tokio::spawn(async move {
        process_get_method_async(subscription).await;
        // subscription is automatically unsubscribed when dropped at end of task
    });
}
