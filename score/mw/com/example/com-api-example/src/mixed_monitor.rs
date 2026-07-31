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

// This file demonstrates the usage of the mixed VehicleMonitorInterface, which combines
// Events, Fields, and Methods in a single interface definition.
//
// VehicleMonitorInterface is defined as:
//   interface VehicleMonitor {
//       Id = "VehicleMonitorInterface",
//       left_tire:  Event<Tire>,                                      // event
//       exhaust:    Event<Exhaust>,                                   // event
//       left_tire_field: Field<Tire, WithGetter + WithSetter + WithNotifier>, // field
//       exhaust_field:   Field<Exhaust, WithGetter + WithSetter + WithNotifier>, // field
//       update_tire_pressure(Tire) -> (),                            // method
//       update_front_tires_pressure(Tire, Tire) -> (),               // method
//       get_tire_pressure() -> Tire,                                 // method
//   }
//
// Producer side (skeleton):
//   - Events are published via `offered.left_tire.send(...)` / `offered.exhaust.send(...)`.
//   - Fields require an initial value (`update_left_tire_field` / `update_exhaust_field`) and
//     a set-handler (`register_set_handler_*_field`) and a get-handler
//     (`register_get_handler_*_field`) before `offer()` is available
//     (both enforced at compile time via type state).
//   - Methods require all handlers to be registered before `offer()` is available (same type state).
//
// Consumer side (proxy):
//   - Events are subscribed via `left_tire.subscribe()` / `exhaust.subscribe()`.
//   - Field notifications are subscribed via `left_tire_field.subscribe()`.
//   - Field get/set are async wrappers backed by MethodCaller:
//       `consumer.get_left_tire_field().await` / `consumer.set_left_tire_field(val).await`
//   - Methods are called as async wrappers: `consumer.update_tire_pressure(tire).await`
//
// This builds fine, but cannot run as field and method APIs in Lola runtime are not implemented yet.

use score_com::{
    Builder, FieldPublisher, FindServiceSpecifier, InstanceSpecifier, Interface, MethodCaller,
    MethodInArgMaybeUninit, Producer, Publisher, Runtime, SampleContainer, ServiceDiscovery,
    Subscriber, Subscription,
};

use com_api_gen::{Exhaust, Tire, VehicleMonitorInterface};

//  Type aliases 

#[allow(dead_code)]
type VehicleMonitorProducer<R> = <VehicleMonitorInterface as Interface>::Producer<R>;

#[allow(dead_code)]
type VehicleMonitorOfferedProducer<R> =
    <<VehicleMonitorInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;

#[allow(dead_code)]
type VehicleMonitorConsumer<R> = <VehicleMonitorInterface as Interface>::Consumer<R>;

//  Producer 

/// Create and offer a VehicleMonitor producer.
///
/// The type-state chain on `init()` enforces at **compile time** that:
/// - every Field has an initial value set (`update_*_field`)
/// - every Field has a set-handler registered (`register_set_handler_*_field`)
/// - every Field has a get-handler registered (`register_get_handler_*_field`)
/// - every Method has a handler registered (`register_*_handler`)
///
/// Calling `offer()` before satisfying all of the above is a **compile error**.
#[allow(dead_code)]
fn create_monitor_producer<R: Runtime + 'static>(
    runtime: &R,
    service_id: InstanceSpecifier,
    initial_tire: Tire,
    initial_exhaust: Exhaust,
) -> VehicleMonitorOfferedProducer<R>
where
    <R as Runtime>::FieldPublisher<Tire>: Send + Sync,
    <R as Runtime>::FieldPublisher<Exhaust>: Send,
{
    let producer = runtime
        .producer_builder::<VehicleMonitorInterface>(service_id)
        .build()
        .expect("Failed to build VehicleMonitor producer");

    producer
        .init()
        //  Field: left_tire_field 
        // Register set-handler: called by the middleware when a consumer calls Set on this field.
        // Receives the accepted value by value for inspection / side effects.
        .register_set_handler_left_tire_field(|val: Tire| {
            println!("[Producer] set_handler left_tire_field: {:?}", val);
            // Additional validation or side-effect logic can go here.
        })
        // Register get-handler: called by the middleware when a consumer calls Get on this field.
        // Required before offer() for WithGetter fields.
        .register_get_handler_left_tire_field(|| Tire { pressure: 32.0 })
        // Set initial field value (required before offer()).
        .update_left_tire_field(initial_tire)
        .expect("Failed to set initial value for left_tire_field")
        //  Field: exhaust_field 
        .register_set_handler_exhaust_field(|val: Exhaust| {
            let _ = val;
            println!("[Producer] set_handler exhaust_field");
        })
        .register_get_handler_exhaust_field(|| Exhaust {})
        .update_exhaust_field(initial_exhaust)
        .expect("Failed to set initial value for exhaust_field")
        //  Method: update_tire_pressure(Tire) -> () 
        .register_update_tire_pressure_handler(|tire: Tire| {
            println!("[Producer] update_tire_pressure called: {:?}", tire);
        })
        //  Method: update_front_tires_pressure(Tire, Tire) -> () 
        .register_update_front_tires_pressure_handler(|tire1: Tire, tire2: Tire| {
            println!(
                "[Producer] update_front_tires_pressure called: {:?}, {:?}",
                tire1, tire2
            );
        })
        //  Method: get_tire_pressure() -> Tire 
        .register_get_tire_pressure_handler(|| {
            println!("[Producer] get_tire_pressure called");
            // Return the current field value; in a real implementation this would
            // read from the last Update()d field value.
            Tire { pressure: 32.0 }
        })
        // All states satisfied → offer() is available.
        .offer()
        .expect("Failed to offer VehicleMonitor producer")
}

/// Demonstrate publishing events on the already-offered producer.
/// Events do not require type-state initialization; they can be published at any time.
#[allow(dead_code)]
fn publish_events<R: Runtime>(offered: &VehicleMonitorOfferedProducer<R>) {
    // Publish an event update for left_tire (event member, not field).
    offered
        .left_tire
        .send(Tire { pressure: 33.5 })
        .expect("Failed to publish left_tire event");

    // Publish an event update for exhaust.
    offered
        .exhaust
        .send(Exhaust {})
        .expect("Failed to publish exhaust event");
}

/// Demonstrate updating field values on the already-offered producer.
#[allow(dead_code)]
fn update_fields<R: Runtime>(offered: &VehicleMonitorOfferedProducer<R>) {
    // Update field value  sends the new value to all field subscribers.
    offered
        .left_tire_field
        .update(Tire { pressure: 34.0 })
        .expect("Failed to update left_tire_field");

    offered
        .exhaust_field
        .update(Exhaust {})
        .expect("Failed to update exhaust_field");
}

//  Consumer 

/// Create a VehicleMonitor consumer by discovering the service instance.
#[allow(dead_code)]
fn create_monitor_consumer<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleMonitorConsumer<R> {
    let discovery = runtime
        .find_service::<VehicleMonitorInterface>(FindServiceSpecifier::Specific(service_id));

    let instances = discovery
        .get_available_instances()
        .expect("Failed to get available service instances");

    instances
        .into_iter()
        .next()
        .expect("No VehicleMonitor service instance found")
        .build()
        .expect("Failed to build VehicleMonitor consumer")
}

/// Demonstrate all consumer-side APIs on the VehicleMonitorInterface:
/// - Subscribe to events and poll for samples.
/// - Async field get/set via generated MethodCaller wrappers.
/// - Subscribe to field notifications and poll for updates.
/// - Async method calls (copy and zero-copy paths).
#[allow(dead_code)]
async fn consume_monitor<R: Runtime>(consumer: VehicleMonitorConsumer<R>) {
    // TODO: Ordering workaround  subscribe(self) vs. whole-struct &self methods
    //
    // The current Subscriber trait signature is:
    //   fn subscribe(self, max_num_samples: usize) -> Result<Self::Subscription>
    //
    // This takes the subscriber by value, which partially moves the corresponding
    // field (e.g. consumer.left_tire) out of the consumer struct. Once any field
    // is partially moved, Rust's borrow checker rejects whole-struct &self borrows
    // such as consumer.get_left_tire_field() or consumer.update_tire_pressure().
    //
    // Workaround Just for example: reorder so all whole-struct &self calls (field get/set, methods)
    // happen FIRST, and all subscribe() calls happen LAST  at that point individual
    // field access (consumer.left_tire_field) still works because Rust tracks partial
    // moves per field, not per struct.
    //
    // TODO Suggest fix: change subscribe to take &mut self:
    //   fn subscribe(&mut self, max_num_samples: usize) -> Result<Self::Subscription>
    // With &mut self, no field is ever moved out, so consumer remains fully usable in any order.
    // With this change, unsubscribe return also need to change.
    
    //  Fields (async get/set) 
    // Async get  uses MethodCaller<(), Tire> under the hood.
    match consumer.get_left_tire_field().await {
        Ok(result) => println!("[Consumer] left_tire_field get: {:?}", *result),
        Err(e) => eprintln!("[Consumer] left_tire_field get failed: {:?}", e),
    }

    // Async set  uses MethodCaller<(Tire,), Tire> under the hood.
    // Returns the accepted value (after the producer's set-handler may have modified it).
    match consumer.set_left_tire_field(Tire { pressure: 36.0 }).await {
        Ok(result) => println!("[Consumer] left_tire_field set confirmed: {:?}", *result),
        Err(e) => eprintln!("[Consumer] left_tire_field set failed: {:?}", e),
    }

    //  Methods 
    // Copy path: single argument.
    match consumer.update_tire_pressure(Tire { pressure: 30.0 }).await {
        Ok(_) => println!("[Consumer] update_tire_pressure OK"),
        Err(e) => eprintln!("[Consumer] update_tire_pressure failed: {:?}", e),
    }

    // Copy path: two arguments.
    match consumer
        .update_front_tires_pressure(Tire { pressure: 31.0 }, Tire { pressure: 32.0 })
        .await
    {
        Ok(_) => println!("[Consumer] update_front_tires_pressure OK"),
        Err(e) => eprintln!("[Consumer] update_front_tires_pressure failed: {:?}", e),
    }

    // Zero-copy path: allocate, write, then call.
    let (uninit,) = consumer
        .update_tire_pressure
        .allocate()
        .expect("Failed to allocate method argument");
    let tire_ptr = uninit.write(Tire { pressure: 35.0 });
    match consumer.update_tire_pressure(tire_ptr).await {
        Ok(_) => println!("[Consumer] update_tire_pressure (zero-copy) OK"),
        Err(e) => eprintln!("[Consumer] update_tire_pressure (zero-copy) failed: {:?}", e),
    }

    // Zero-argument method returning a value.
    match consumer.get_tire_pressure().await {
        Ok(tire) => println!("[Consumer] get_tire_pressure: {:?}", *tire),
        Err(e) => eprintln!("[Consumer] get_tire_pressure failed: {:?}", e),
    }

    //  Events 
    // subscribe(self) moves consumer.left_tire out of consumer (partial move).
    // Whole-struct &self methods are not allowed after this point, but direct
    // field access to other fields (e.g. consumer.left_tire_field below) still works.
    {
        let event_subscription = consumer
            .left_tire
            .subscribe(4)
            .expect("Failed to subscribe to left_tire event");

        let mut event_buf = SampleContainer::new(4);
        match event_subscription.try_receive(&mut event_buf, 4) {
            Ok(n) if n > 0 => {
                while let Some(sample) = event_buf.pop_front() {
                    println!("[Consumer] left_tire event: {:?}", *sample);
                }
            }
            _ => println!("[Consumer] No new left_tire event samples"),
        }
        // Drop event_buf before event_subscription: Sample<'_> borrows from the
        // subscription, so the buffer must be gone before the subscription is dropped.
        drop(event_buf);
        // event_subscription dropped here (unsubscribed).
    }

    //  Fields (subscribe for notifications) 
    // consumer.left_tire is partially moved above, but consumer.left_tire_field is a
    // distinct field and is still valid  Rust tracks field moves individually.
    {
        let field_subscription = consumer
            .left_tire_field
            .subscribe(3)
            .expect("Failed to subscribe to left_tire_field notifications");

        let mut field_buf = SampleContainer::new(3);
        match field_subscription.try_receive(&mut field_buf, 3) {
            Ok(n) if n > 0 => {
                while let Some(sample) = field_buf.pop_front() {
                    println!("[Consumer] left_tire_field notification: {:?}", *sample);
                }
            }
            _ => println!("[Consumer] No new left_tire_field notifications"),
        }
        drop(field_buf);
        // field_subscription dropped here (unsubscribed).
    }

    //  TODO: Uncomment when Runtime implementation is ready and 
    //  subscribe() is changed to take &mut self (no partial move).
    // match consumer.update_tire_pressure(Tire { pressure: 30.0 }).await {
    //     Ok(_) => println!("[Consumer] update_tire_pressure OK"),
    //     Err(e) => eprintln!("[Consumer] update_tire_pressure failed: {:?}", e),
    // }

}
