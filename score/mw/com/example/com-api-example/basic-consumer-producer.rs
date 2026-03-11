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

use com_api::{
    Builder, Error, FindServiceSpecifier, InstanceSpecifier, Interface, LolaRuntimeBuilderImpl,
    OfferedProducer, Producer, Publisher, Result, Runtime, RuntimeBuilder, SampleContainer,
    SampleMaybeUninit, SampleMut, ServiceDiscovery, Subscriber, Subscription,
};

use com_api_gen::{Exhaust, Tire, VehicleInterface};

// Type aliases for generated consumer and offered producer types for the Vehicle interface
// VehicleConsumer is the consumer type generated for the Vehicle interface, parameterized by the runtime R
type VehicleConsumer<R> = <VehicleInterface as Interface>::Consumer<R>;
// VehicleOfferedProducer is the offered producer type generated for the Vehicle interface, parameterized by the runtime R
type VehicleOfferedProducer<R> =
    <<VehicleInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;

// Example struct demonstrating composition with VehicleConsumer
pub struct VehicleMonitor<R: Runtime> {
    producer: VehicleOfferedProducer<R>,
    tire_subscriber: <<R as Runtime>::Subscriber<Tire> as Subscriber<Tire, R>>::Subscription,
    _exhaust_subscriber:
        <<R as Runtime>::Subscriber<Exhaust> as Subscriber<Exhaust, R>>::Subscription,
}

impl<R: Runtime> VehicleMonitor<R> {
    /// Create a new `VehicleMonitor` with a consumer
    ///
    /// # Returns
    ///
    /// A `Result` containing the monitor on success or an error if subscription fails.
    ///
    /// # Errors
    ///
    /// Returns an error if subscribing to the tire data fails.
    ///
    /// # Panics
    ///
    /// Panics if unwrapping the subscription fails.
    pub fn new(consumer: VehicleConsumer<R>, producer: VehicleOfferedProducer<R>) -> Result<Self> {
        let tire_subscriber = consumer.left_tire.subscribe(3).unwrap();
        let exhaust_subscriber = consumer.exhaust.subscribe(3).unwrap();
        Ok(Self {
            producer,
            tire_subscriber,
            _exhaust_subscriber: exhaust_subscriber,
        })
    }

    /// Monitor tire data from the consumer
    ///
    /// # Returns
    ///
    /// A `Result` containing tire data as a `String` on success or an error if reading fails.
    ///
    /// # Errors
    ///
    /// Returns an error if no data is received or if there is a failure in receiving data
    ///
    /// # Panics
    ///
    /// Panics if unwrapping the sample from the buffer fails.
    pub fn read_tire_data(&self) -> Result<String> {
        let mut sample_buf = SampleContainer::new(3);

        match self.tire_subscriber.try_receive(&mut sample_buf, 1) {
            Ok(0) => Err(Error::Fail),
            Ok(x) => {
                let sample = sample_buf.pop_front().unwrap();
                Ok(format!("{} samples received: sample[0] = {:?}", x, *sample))
            }
            Err(e) => Err(e),
        }
    }

    /// Write tire data using the producer
    /// # Returns
    ///
    /// A `Result` indicating success or failure of the write operation.
    ///
    /// # Errors
    ///
    /// Returns an error if allocation or sending of the sample fails.
    pub fn write_tire_data(&self, tire: Tire) -> Result<()> {
        // Allocate API of lola is not calling at the moment
        let uninit_sample = self.producer.left_tire.allocate()?;
        let sample = uninit_sample.write(tire);
        sample.send()?;
        println!("Tire data sent");
        Ok(())
    }
}

// Create a consumer for the specified service identifier
fn create_consumer<R: Runtime>(runtime: &R, service_id: InstanceSpecifier) -> VehicleConsumer<R> {
    let consumer_discovery =
        runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
    let available_service_instances = consumer_discovery.get_available_instances().unwrap();

    // Select service instance at specific handle_index
    let handle_index = 0; // or any index you need from vector of instances
    let consumer_builder = available_service_instances
        .into_iter()
        .nth(handle_index)
        .unwrap();

    consumer_builder.build().unwrap()
}

#[allow(dead_code)]
//it is used in async test, but to avoid unused code warning in main example, it is marked as allow(dead_code)
async fn create_consumer_async<R: Runtime>(
    runtime: R,
    service_id: InstanceSpecifier,
) -> VehicleConsumer<R> {
    let consumer_discovery =
        runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
    let available_service_instances = consumer_discovery
        .get_available_instances_async()
        .await
        .unwrap();

    // Select service instance at specific handle_index
    let handle_index = 0; // or any index you need from vector of instances
    let consumer_builder = available_service_instances
        .into_iter()
        .nth(handle_index)
        .unwrap();

    consumer_builder.build().unwrap()
}

// Create a producer for the specified service identifier
fn create_producer<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleOfferedProducer<R> {
    let producer_builder = runtime.producer_builder::<VehicleInterface>(service_id);
    let producer = producer_builder.build().unwrap();
    producer.offer().unwrap()
}

// Run the example with the specified runtime
fn run_with_runtime<R: Runtime>(name: &str, runtime: &R) {
    println!("\n=== Running with {name} runtime ===");

    let service_id = InstanceSpecifier::new("/Vehicle/Service1/Instance")
        .expect("Failed to create InstanceSpecifier");
    let producer = create_producer(runtime, service_id.clone());
    let consumer = create_consumer(runtime, service_id);
    let monitor = VehicleMonitor::new(consumer, producer).unwrap();
    let tire_pressure = 5.0;
    println!("Setting tire pressure to {tire_pressure}");
    for i in 0..5 {
        monitor
            .write_tire_data(Tire {
                pressure: tire_pressure + i as f32,
            })
            .unwrap();
        let tire_data = monitor.read_tire_data().unwrap();
        println!("{tire_data}");
    }
    //unoffer returns producer back, so if needed it can be used further
    match monitor.producer.unoffer() {
        Ok(_) => println!("Successfully unoffered the service"),
        Err(e) => eprintln!("Failed to unoffer: {:?}", e),
    }
    println!("=== {name} runtime completed ===\n");
}

// Initialize Lola runtime builder with configuration
fn init_lola_runtime_builder() -> LolaRuntimeBuilderImpl {
    let mut lola_runtime_builder = LolaRuntimeBuilderImpl::new();
    lola_runtime_builder.load_config(std::path::Path::new(
        "score/mw/com/example/com-api-example/etc/config.json",
    ));
    lola_runtime_builder
}

fn main() {
    let lola_runtime_builder = init_lola_runtime_builder();
    let lola_runtime = lola_runtime_builder.build().unwrap();
    run_with_runtime("Lola", &lola_runtime);
}

#[cfg(test)]
mod test {
    use super::*;
    use std::thread;
    use std::time::Duration;

    #[test]
    fn integration_test() {
        println!("Starting integration test with Lola runtime");
        let lola_runtime_builder = init_lola_runtime_builder();
        let lola_runtime = lola_runtime_builder.build().unwrap();
        run_with_runtime("Lola", &lola_runtime);
    }

    // Test case: Async sender and receiver on separate threads
    // Demonstrates true concurrent async operations on different threads
    // Each thread gets its own runtime instance
    // It use futures-based blocking executor to run async code in each thread,
    #[test]
    fn test_async_sender_receiver_threads() {
        println!("=== Starting async sender and receiver on separate threads ===");

        let service_id = InstanceSpecifier::new("/Vehicle/Service2/Instance")
            .expect("Failed to create InstanceSpecifier");

        // Sender thread
        let service_id_sender = service_id.clone();
        let sender_handle = std::thread::spawn(move || {
            // Each thread creates its own runtime instance
            let lola_runtime_builder = init_lola_runtime_builder();
            let lola_runtime = lola_runtime_builder.build().unwrap();

            let producer = create_producer(&lola_runtime, service_id_sender);

            println!("[SENDER] Thread started: {:?}", thread::current().id());

            futures::executor::block_on(async {
                for i in 0..5 {
                    let tire = Tire {
                        pressure: 1.0 + (i as f32 * 0.5),
                    };
                    println!("[SENDER] Sending sample {}: {:.2} psi", i, tire.pressure);

                    let uninit_sample = producer.left_tire.allocate().unwrap();
                    let sample = uninit_sample.write(tire);
                    sample.send().unwrap();

                    // Simulate async work delay
                    std::thread::sleep(Duration::from_millis(1000));
                }
                println!("[SENDER] All samples sent");
            });
        });

        let service_id_receiver = service_id.clone();
        // Receiver thread
        let receiver_handle = std::thread::spawn(move || {
            // Ensure sender starts first
            std::thread::sleep(Duration::from_millis(500));
            // Each thread creates its own runtime instance
            let lola_runtime_builder = init_lola_runtime_builder();
            let lola_runtime = lola_runtime_builder.build().unwrap();

            let consumer = create_consumer(&lola_runtime, service_id_receiver);
            let subscribed = consumer.left_tire.subscribe(5).unwrap();

            println!("[RECEIVER] Thread started: {:?}", thread::current().id());

            futures::executor::block_on(async {
                let mut sample_buf = SampleContainer::new(5);
                let mut total_received = 0;
                const MAX_ATTEMPTS: usize = 5;

                for attempt in 0..MAX_ATTEMPTS {
                    println!("[RECEIVER] Attempt {}", attempt);

                    // Match and immediately reassign in all branches
                    sample_buf = match subscribed.receive(sample_buf, 1, 3).await {
                        Ok(returned_buf) => {
                            let count = returned_buf.sample_count();

                            if count > 0 {
                                total_received += count;
                                println!(
                                    "[RECEIVER] Received {} samples (total: {})",
                                    count, total_received
                                );

                                // Create a mutable version to pop from
                                let mut buf = returned_buf;
                                while let Some(sample) = buf.pop_front() {
                                    println!("[RECEIVER]   Sample: {:.2} psi", sample.pressure);
                                }
                                buf
                            } else {
                                returned_buf
                            }
                        }
                        Err(e) => {
                            println!("[RECEIVER] Error on attempt {}: {:?}", attempt, e);
                            // Create a fresh buffer if there's an error
                            SampleContainer::new(5)
                        }
                    };
                }

                println!("[RECEIVER] Total received: {}", total_received);
            });
        });

        // Wait for both threads
        sender_handle.join().expect("Sender thread panicked");
        receiver_handle.join().expect("Receiver thread panicked");
        println!("=== Async sender and receiver threads test completed ===");
    }

    //sender will send data in each 2 milliseconds
    async fn async_data_sender_fn<R: Runtime>(
        offered_producer: VehicleOfferedProducer<R>,
    ) -> VehicleOfferedProducer<R> {
        for i in 0..10 {
            let uninit_sample = offered_producer.left_tire.allocate().unwrap();
            let sample = uninit_sample.write(Tire {
                pressure: 1.0 + i as f32,
            });
            sample.send().unwrap();
            println!(
                "[SENDER] Sent sample with pressure: {:.2} psi",
                1.0 + i as f32
            );
            tokio::time::sleep(tokio::time::Duration::from_millis(2000)).await;
        }
        offered_producer
    }

    //receiver function which use async receive to get data, it waits for new data and process it once it arrives,
    //it will receive data 10 times and print the received samples
    async fn async_data_processor_fn<R: Runtime>(subscribed: impl Subscription<Tire, R>) {
        println!("[RECEIVER] Async data processor started");
        let mut buffer = SampleContainer::new(5);
        for _ in 0..5 {
            buffer = match subscribed.receive(buffer, 2, 3).await {
                Ok(returned_buf) => {
                    let count = returned_buf.sample_count();
                    if count > 0 {
                        println!("[RECEIVER] Received {} samples", count);
                        let mut buf = returned_buf;
                        while let Some(sample) = buf.pop_front() {
                            println!("[RECEIVER]   Sample: {:.2} psi", sample.pressure);
                        }
                        buf
                    } else {
                        returned_buf
                    }
                }
                Err(e) => {
                    println!("[RECEIVER] Error receiving data: {:?}", e);
                    SampleContainer::new(5)
                }
            }
        }
    }

    // Test case: Async subscription and sending on multi-threaded runtime
    // Use the tokio multi-threaded runtime to run async sender and receiver concurrently on separate threads
    #[tokio::test(flavor = "multi_thread")]
    async fn receive_and_send_using_multi_thread() {
        println!("Starting async subscription test with Lola runtime");
        let service_id = InstanceSpecifier::new("/Vehicle/Service3/Instance")
            .expect("Failed to create InstanceSpecifier");
        let service_id_clone = service_id.clone();
        //consumer create
        let lola_runtime_builder = LolaRuntimeBuilderImpl::new();
        let lola_runtime = lola_runtime_builder.build().unwrap();
        //starting service discovery in async way, so that it can be discovered when producer offer service after some delay, and consumer is waiting for discovery result
        let consumer = tokio::spawn(create_consumer_async(lola_runtime, service_id));
        //simulate some delay before producer offer service, so that consumer is waiting for discovery
        tokio::time::sleep(tokio::time::Duration::from_millis(1000)).await;
        //Producer create
        let lola_runtime_builder_ = LolaRuntimeBuilderImpl::new();
        let lola_runtime_ = lola_runtime_builder_.build().unwrap();
        let producer = create_producer(&lola_runtime_, service_id_clone);
        // Spawn async data sender
        let sender_join_handle = tokio::spawn(async_data_sender_fn(producer));
        // Await consumer creation and subscribe to events
        let consumer = consumer.await.expect("Failed to create consumer");
        // Subscribe to one event
        let subscribed = consumer.left_tire.subscribe(5).unwrap();
        // Spawn async data processor
        let processor_join_handle = tokio::spawn(async_data_processor_fn(subscribed));
        processor_join_handle
            .await
            .expect("Error returned from task");
        let producer = sender_join_handle.await.expect("Error returned from task");

        match producer.unoffer() {
            Ok(_) => println!("Successfully unoffered the service"),
            Err(e) => eprintln!("Failed to unoffer: {:?}", e),
        }

        println!("=== Async subscription test with Lola runtime completed ===\n");
    }
}
