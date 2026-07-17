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

// This demo app writing and reading tire pressure data using producer and consumer respectively.
// It is demonstrating the composition of consumer and producer in one struct,
// but they can be used separately as well.
// The example is using Lola runtime, but it can be used with any runtime by changing the runtime initialization part.
// Note: The example is using unwrap and panic in some places for simplicity,
// but it is recommended to handle errors properly in production code.

#![allow(unused)]

use clap::Parser;
use std::path::PathBuf;

use com_api::{
    Builder, FindServiceSpecifier, InstanceSpecifier, Interface, LolaRuntimeBuilderImpl,
    MethodCaller, MethodInArgMaybeUninit, OfferedProducer, Producer, Publisher, Result, Runtime,
    RuntimeBuilder, SampleContainer, SampleMaybeUninit, SampleMut, ServiceDiscovery, Subscriber,
    Subscription,
};

use com_api_gen::{Exhaust, Tire, VehicleInterface, VehicleMethodsInterface};

#[derive(Parser)]
struct Arguments {
    #[arg(
        short,
        long,
        default_value = "./score/mw/com/example/com-api-example/etc/mw_com_config.json"
    )]
    service_instance_manifest: PathBuf,
}

// Type aliases for generated consumer and offered producer types for the Vehicle interface
// VehicleConsumer is the consumer type generated for the Vehicle interface, parameterized by the runtime R
type VehicleConsumer<R> = <VehicleInterface as Interface>::Consumer<R>;
// VehicleOfferedProducer is the offered producer type generated for the Vehicle interface, parameterized by the runtime R
type VehicleOfferedProducer<R> =
    <<VehicleInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;

type VehicleMethodConsumer<R> = <VehicleMethodsInterface as Interface>::Consumer<R>;

type VehicleMethodOfferedProducer<R> =
    <<VehicleMethodsInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;

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
            Ok(0) => Ok("No tire data received".to_string()),
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

// TODO: Currently all the method is synchronous, but in future we can add async version of method call as well, if needed.
// We are having tuple of arguments, so we can have any number of arguments (currently up to 2) without any extra boilerplate.
// But in Method signature, we need to have tuple of arguments, so for zero argument method, we need to pass empty tuple.
// Which need to be improved using macro generated wrapper around method call, so that we can call zero argument method without empty tuple.
// even with argument tuple, method call can be improve using macro generated wrapper, so that we can call method with any number of arguments without tuple.

fn consumer_method_processing<R: Runtime>(consumer: VehicleMethodConsumer<R>) {
    // Call the update_tire_pressure method with a sample tire pressure value
    let tire = Tire { pressure: 30.0 };
    match consumer.update_tire_pressure((tire,)) {
        Ok(_) => println!("Successfully called update_tire_pressure method"),
        Err(e) => eprintln!("Failed to call update_tire_pressure method: {:?}", e),
    }

    let (uninit1,) = consumer
        .update_tire_pressure
        .allocate()
        .expect("Failed to allocate method arguments");
    let tire1ptr = uninit1.write(Tire { pressure: 35.0 });

    // Call the get_tire_pressure method to retrieve the current tire pressure
    // Note: Currently we need to pass empty tuple for zero argument method,
    // but in future we can change the API to not require empty tuple for zero argument method
    // Maybe by implementing wrapper around in interface macro.
    match consumer.get_tire_pressure(()) {
        Ok(tire) => println!("Current tire pressure: {:?}", tire),
        Err(e) => eprintln!("Failed to call get_tire_pressure method: {:?}", e),
    }

    // Same method name as copy call - MethodCallInput dispatches to zero-copy path via 1-tuple ptr type
    match consumer.update_tire_pressure((tire1ptr,)) {
        Ok(_) => println!("Successfully called update_tire_pressure method with allocated args"),
        Err(e) => eprintln!(
            "Failed to call update_tire_pressure method with allocated args: {:?}",
            e
        ),
    }

    let tire1 = Tire { pressure: 31.0 };
    let tire2 = Tire { pressure: 32.0 };
    match consumer.update_front_tires_pressure((tire1, tire2)) {
        Ok(_) => println!("Successfully called update_front_tires_pressure method"),
        Err(e) => eprintln!("Failed to call update_front_tires_pressure method: {:?}", e),
    }

    let (uninit1, uninit2) = consumer
        .update_front_tires_pressure
        .allocate()
        .expect("Failed to allocate method arguments");
    let tire1ptr = uninit1.write(Tire { pressure: 36.0 });
    let tire2ptr = uninit2.write(Tire { pressure: 37.0 });
    // Same method name as copy call - tuple of MethodInArgPtr dispatches to zero-copy path
    match consumer.update_front_tires_pressure((tire1ptr, tire2ptr)) {
        Ok(_) => {
            println!("Successfully called update_front_tires_pressure method with allocated args")
        }
        Err(e) => eprintln!(
            "Failed to call update_front_tires_pressure method with allocated args: {:?}",
            e
        ),
    }
}

fn create_producer_method<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleMethodOfferedProducer<R> {
    let producer_builder = runtime.producer_builder::<VehicleMethodsInterface>(service_id);
    let producer = producer_builder
        .build()
        .expect("Failed to build producer instance");
    producer
        .init_handlers()
        .register_update_tire_pressure_handler(|tire: Tire| {
            println!("Received update_tire_pressure call with tire: {:?}", tire);
            ()
        })
        .expect("Failed to register update_tire_pressure handler")
        .register_get_tire_pressure_handler(|| {
            println!("Received get_tire_pressure call");
            // Return a sample tire pressure value, just dummy value returned for demonstration
            Tire { pressure: 32.0 }
        })
        .expect("Failed to register get_tire_pressure handler")
        .register_update_front_tires_pressure_handler(|tire1: Tire, tire2: Tire| {
            println!(
                "Received update_front_tires_pressure call with tire1: {:?}, tire2: {:?}",
                tire1, tire2
            );
            ()
        })
        .expect("Failed to register update_front_tires_pressure handler")
        .offer()
        .expect("Failed to offer producer instance")
}

// Create a consumer for the specified service identifier
fn create_consumer<R: Runtime>(runtime: &R, service_id: InstanceSpecifier) -> VehicleConsumer<R> {
    let consumer_discovery =
        runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
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

#[allow(dead_code)]
//it is used in async test, but to avoid unused code warning in main example, it is marked as allow(dead_code)
async fn create_consumer_async<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleConsumer<R> {
    let consumer_discovery =
        runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
    let available_service_instances = consumer_discovery
        .get_available_instances_async()
        .await
        .expect("Failed to get available service instances asynchronously");

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

// Create a producer for the specified service identifier
fn create_producer<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleOfferedProducer<R> {
    let producer_builder = runtime.producer_builder::<VehicleInterface>(service_id);
    let producer = producer_builder
        .build()
        .expect("Failed to build producer instance");
    producer.offer().expect("Failed to offer producer instance")
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
        match monitor.write_tire_data(Tire {
            pressure: tire_pressure + i as f32,
        }) {
            Ok(_) => (),
            Err(e) => {
                eprintln!("Failed to write tire data: {:?}", e);
                continue;
            }
        }
        let tire_data = match monitor.read_tire_data() {
            Ok(data) => data,
            Err(e) => {
                eprintln!("Failed to read tire data: {:?}", e);
                continue;
            }
        };
        println!("{tire_data}");
    }
    //unoffer returns producer back, so if needed it can be used further
    match monitor.producer.unoffer() {
        Ok(_) => println!("Successfully unoffered the service"),
        Err(e) => eprintln!("Failed to unoffer: {:?}", e),
    }

    //UnSubscribe the event
    let _ = monitor.tire_subscriber.unsubscribe();
    println!("=== {name} runtime completed ===\n");
}

// Initialize Lola runtime builder with configuration
fn init_lola_runtime_builder(config_path: &std::path::Path) -> LolaRuntimeBuilderImpl {
    let mut lola_runtime_builder = LolaRuntimeBuilderImpl::new();
    if config_path.exists() {
        lola_runtime_builder.load_config(config_path);
    } else {
        eprintln!(
            "Provided config path does not exist: {}",
            config_path.display()
        );
    }
    lola_runtime_builder
}

fn main() {
    let args = Arguments::parse();
    let lola_runtime_builder = init_lola_runtime_builder(&args.service_instance_manifest);
    let lola_runtime = lola_runtime_builder.build().unwrap();
    run_with_runtime("Lola", &lola_runtime);
}

#[cfg(test)]
mod test {
    use super::*;
    use futures::stream::StreamExt;
    use std::sync::OnceLock;
    use std::thread;
    use std::time::Duration;
    const TEST_CONFIG_PATH: &str = "./score/mw/com/example/com-api-example/etc/mw_com_config.json";

    static LOLA_RUNTIME: OnceLock<com_api::LolaRuntimeImpl> = OnceLock::new();
    // it will create a singleton instance of LolaRuntime for testing,
    // and it will be shared across different test cases,
    // so that the runtime initialization is done only once for all tests,
    // and re-initialization of backend is avoided,
    // as it prints warning if backend is initialized more than once.
    fn get_test_runtime() -> &'static com_api::LolaRuntimeImpl {
        LOLA_RUNTIME.get_or_init(|| {
            let lola_runtime_builder =
                init_lola_runtime_builder(std::path::Path::new(TEST_CONFIG_PATH));
            lola_runtime_builder.build().unwrap()
        })
    }

    #[test]
    fn integration_test() {
        println!("Starting integration test with Lola runtime");
        run_with_runtime("Lola", get_test_runtime());
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
            let lola_runtime = get_test_runtime();

            let producer = create_producer(lola_runtime, service_id_sender);

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
                    std::thread::sleep(Duration::from_millis(500));
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
            let lola_runtime = get_test_runtime();

            let consumer = create_consumer(lola_runtime, service_id_receiver);
            let subscribed = consumer.left_tire.subscribe(5).unwrap();

            println!("[RECEIVER] Thread started: {:?}", thread::current().id());

            futures::executor::block_on(async {
                let mut sample_buf = SampleContainer::new(5);
                let mut total_received = 0;
                const MAX_ATTEMPTS: usize = 5;

                for attempt in 0..MAX_ATTEMPTS {
                    println!("[RECEIVER] Attempt {}", attempt);

                    // Destructure tuple - container is always returned even on error
                    let (returned_buf, result) = subscribed.receive(sample_buf, 1, 3).await;
                    sample_buf = {
                        let count = returned_buf.sample_count();
                        if let Err(e) = result {
                            println!("[RECEIVER] Error on attempt {}: {:?}", attempt, e);
                        } else if count > 0 {
                            total_received += count;
                            println!(
                                "[RECEIVER] Received {} samples (total: {})",
                                count, total_received
                            );
                        }
                        // Drain printed samples
                        let mut buf = returned_buf;
                        while let Some(sample) = buf.pop_front() {
                            println!("[RECEIVER]   Sample: {:.2} psi", sample.pressure);
                        }
                        buf
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

    //sender will send data in each 1 second
    async fn async_data_sender_fn<R: Runtime>(
        offered_producer: VehicleOfferedProducer<R>,
    ) -> VehicleOfferedProducer<R> {
        for i in 0..6 {
            let uninit_sample = match offered_producer.left_tire.allocate() {
                Ok(sample) => sample,
                Err(e) => {
                    eprintln!("[SENDER] Failed to allocate sample: {:?}", e);
                    continue;
                }
            };
            let sample = uninit_sample.write(Tire {
                pressure: 1.0 + i as f32,
            });
            match sample.send() {
                Ok(_) => (),
                Err(e) => eprintln!("[SENDER] Failed to send sample: {:?}", e),
            }
            println!("[SENDER] Sent sample with pressure: {}", 1.0 + i as f32);
            tokio::time::sleep(tokio::time::Duration::from_millis(1000)).await;
        }
        offered_producer
    }

    //receiver function which use async receive to get data, it waits for new data and process it once it arrives,
    //it will receive data 10 times and print the received samples
    async fn async_data_processor_fn<R: Runtime>(
        subscribed: impl Subscription<Tire, R>,
        is_timeout: bool,
    ) {
        println!("[RECEIVER] Async data processor started");
        let mut buffer = SampleContainer::new(5);
        for _ in 0..3 {
            let (returned_buf, result) = if is_timeout {
                let timeout = tokio::time::sleep(Duration::from_millis(1000));
                subscribed.cancellable_receive(buffer, 2, 3, timeout).await
            } else {
                subscribed.receive(buffer, 2, 3).await
            };
            match result {
                Ok(count) => println!("[RECEIVER] Received {} samples", count),
                Err(e) => eprintln!("[RECEIVER] Failed to receive samples: {:?}", e),
            }
            let mut buf = returned_buf;
            while let Some(sample) = buf.pop_front() {
                println!("[RECEIVER]   Sample: {:.2} psi", sample.pressure);
            }
            buffer = buf;
        }
    }

    async fn stream_processor_fn<R: Runtime>(mut subscribed: impl Subscription<Tire, R>) {
        let mut stream = subscribed.to_stream();
        let mut cnt = 5usize;
        println!("[RECEIVER] Stream processor started");
        while cnt > 0 {
            // Use timeout to avoid waiting indefinitely in case of issues with the producer or subscription
            match tokio::time::timeout(tokio::time::Duration::from_secs(3), stream.next()).await {
                Ok(Some(Ok(sample))) => {
                    println!(
                        "[RECEIVER] Stream received sample: {:.2} psi",
                        sample.pressure
                    )
                }
                Ok(Some(Err(e))) => eprintln!("[RECEIVER] Stream error: {:?}", e),
                Ok(None) => break,
                Err(_) => {
                    eprintln!("[RECEIVER] Timeout while waiting for stream sample");
                    break;
                }
            }
            cnt -= 1;
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
        let consumer_runtime = get_test_runtime();
        //starting service discovery in async way, so that it can be discovered when producer offer service after some delay, and consumer is waiting for discovery result
        let consumer = tokio::spawn(create_consumer_async(consumer_runtime, service_id));
        //simulate some delay before producer offer service, so that consumer is waiting for discovery
        tokio::time::sleep(tokio::time::Duration::from_millis(1000)).await;
        //Producer create
        let producer_runtime = get_test_runtime();
        let producer = create_producer(producer_runtime, service_id_clone);
        // Spawn async data sender
        let sender_join_handle = tokio::spawn(async_data_sender_fn(producer));
        // Await consumer creation and subscribe to events
        let consumer = consumer.await.expect("Failed to create consumer");
        // Subscribe to one event
        let subscribed = consumer.left_tire.subscribe(5).unwrap();

        let processor_join_handle = tokio::spawn(async_data_processor_fn(subscribed, false));
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

    #[tokio::test(flavor = "multi_thread")]
    #[ignore = "This test demonstrates async receive with timeout, it can run individually if wanted to test timeout behavior"]
    async fn receive_with_timeout_and_send_using_multi_thread() {
        println!("Starting async subscription test with Lola runtime");
        //Intentionally using service instance of test1, if you face issue add new service instance in config file and use it here.
        let service_id = InstanceSpecifier::new("/Vehicle/Service1/Instance")
            .expect("Failed to create InstanceSpecifier");
        let service_id_clone = service_id.clone();
        //consumer create
        let consumer_runtime = get_test_runtime();
        //starting service discovery in async way, so that it can be discovered when producer offer service after some delay, and consumer is waiting for discovery result
        let consumer = tokio::spawn(create_consumer_async(consumer_runtime, service_id));
        //simulate some delay before producer offer service, so that consumer is waiting for discovery
        tokio::time::sleep(tokio::time::Duration::from_millis(1000)).await;
        //Producer create
        let producer_runtime = get_test_runtime();
        let producer = create_producer(producer_runtime, service_id_clone);
        // Spawn async data sender
        let sender_join_handle = tokio::spawn(async_data_sender_fn(producer));
        // Await consumer creation and subscribe to events
        let consumer = consumer.await.expect("Failed to create consumer");
        // Subscribe to one event
        let subscribed = consumer.left_tire.subscribe(5).unwrap();

        let processor_join_handle = tokio::spawn(async_data_processor_fn(subscribed, true));
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

    // Test case: Async subscription and sending on multi-threaded runtime
    // Use the tokio multi-threaded runtime to run async sender and receiver concurrently on separate threads
    #[tokio::test(flavor = "multi_thread")]
    async fn stream_and_send_using_multi_thread() {
        println!("Starting async subscription test with Lola runtime");
        let service_id = InstanceSpecifier::new("/Vehicle/Service1/Instance")
            .expect("Failed to create InstanceSpecifier");
        let service_id_clone = service_id.clone();
        //consumer create
        let consumer_runtime = get_test_runtime();
        //starting service discovery in async way, so that it can be discovered when producer offer service after some delay, and consumer is waiting for discovery result
        let consumer = tokio::spawn(create_consumer_async(consumer_runtime, service_id));
        //simulate some delay before producer offer service, so that consumer is waiting for discovery
        tokio::time::sleep(tokio::time::Duration::from_millis(1000)).await;
        //Producer create
        let producer_runtime = get_test_runtime();
        let producer = create_producer(producer_runtime, service_id_clone);
        // Spawn async data sender
        let sender_join_handle = tokio::spawn(async_data_sender_fn(producer));
        // Await consumer creation and subscribe to events
        let consumer = consumer.await.expect("Failed to create consumer");
        // Subscribe to one event
        let subscribed = consumer.left_tire.subscribe(5).unwrap();
        let stream_processor_join_handle = tokio::spawn(stream_processor_fn(subscribed));
        stream_processor_join_handle
            .await
            .expect("Error returned from stream processor task");

        let producer = sender_join_handle.await.expect("Error returned from task");

        match producer.unoffer() {
            Ok(_) => println!("Successfully unoffered the service"),
            Err(e) => eprintln!("Failed to unoffer: {:?}", e),
        }

        println!("=== Async subscription test with Lola runtime completed ===\n");
    }
}
