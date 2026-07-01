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

//! Integration tests for the Vehicle Monitor API example using Tokio runtime.
//!
//! This file contains comprehensive integration tests demonstrating:
//! - Multi-threaded async producer/consumer patterns
//! - Tokio runtime integration for async operations
//! - Asynchronous receive with and without timeout
//! - Stream-based data processing
//! - Concurrent service discovery patterns
//! - Spawn the async operation using tokio::spawn to run them concurrently on separate task/threads.
//!
//! To run these tests:
//! ```bash
//! bazel test //score/mw/com/example/com-api-example:com-api-example-tokio-integration-test
//! ```
//!
//! Note: These tests are tagged as 'manual' and use a shared runtime instance
//! to avoid re-initialization warnings.

#[cfg(test)]
mod test {
    use com_api::{
        Builder, FindServiceSpecifier, InstanceSpecifier, LolaRuntimeBuilderImpl, OfferedProducer,
        Producer, Publisher, Runtime, RuntimeBuilder, SampleContainer, SampleMaybeUninit,
        SampleMut, ServiceDiscovery, Subscriber, Subscription,
    };
    use com_api_gen::{Tire, VehicleInterface, VehicleOfferedProducer};

    use futures::stream::StreamExt;
    use std::sync::OnceLock;

    const TEST_CONFIG_PATH: &str = "./score/mw/com/example/com-api-example/etc/mw_com_config.json";
    const INITIAL_TIRE_PRESSURE: f32 = 1.0;
    const SAMPLE_COUNT: usize = 5;
    const SAMPLE_INTERVAL_MS: u64 = 1000;
    const RECEIVE_TIMEOUT_MS: u64 = 500;
    const TIMEOUT_FOR_SIMULATION_MS: u64 = 1000;
    const PRODUCER_ITERATIONS: usize = 5;
    const CONSUMER_ITERATIONS: usize = 5;
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

    // Create a producer for testing
    fn create_producer<R: Runtime>(
        runtime: &R,
        service_id: InstanceSpecifier,
    ) -> com_api_gen::VehicleOfferedProducer<R> {
        let producer_builder = runtime.producer_builder::<VehicleInterface>(service_id);
        let producer = producer_builder
            .build()
            .expect("Failed to build producer instance");
        producer.offer().expect("Failed to offer producer instance")
    }

    // Create a consumer asynchronously for testing
    async fn create_consumer_async<R: Runtime>(
        runtime: &R,
        service_id: InstanceSpecifier,
    ) -> com_api_gen::VehicleConsumer<R> {
        let consumer_discovery =
            runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
        let available_service_instances = consumer_discovery
            .get_available_instances_async()
            .await
            .expect("Failed to get available service instances asynchronously");

        let handle_index = 0;
        let consumer_builder = available_service_instances
            .into_iter()
            .nth(handle_index)
            .expect("Failed to get consumer builder at specified handle index");

        consumer_builder
            .build()
            .expect("Failed to build consumer instance")
    }

    //sender will send data in each 1 second
    async fn async_data_sender_fn<R: Runtime>(
        offered_producer: VehicleOfferedProducer<R>,
    ) -> VehicleOfferedProducer<R> {
        for i in 0..PRODUCER_ITERATIONS {
            let uninit_sample = match offered_producer.left_tire.allocate() {
                Ok(sample) => sample,
                Err(e) => {
                    eprintln!("[SENDER] Failed to allocate sample: {:?}", e);
                    continue;
                }
            };
            let sample = uninit_sample.write(Tire {
                pressure: INITIAL_TIRE_PRESSURE + i as f32,
            });
            match sample.send() {
                Ok(_) => (),
                Err(e) => eprintln!("[SENDER] Failed to send sample: {:?}", e),
            }
            println!("[SENDER] Sent sample with pressure: {}", 1.0 + i as f32);
            tokio::time::sleep(tokio::time::Duration::from_millis(SAMPLE_INTERVAL_MS)).await;
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
        let mut buffer = SampleContainer::new(SAMPLE_COUNT);
        for _ in 0..CONSUMER_ITERATIONS {
            let (returned_buf, result) = if is_timeout {
                let timeout =
                    tokio::time::sleep(tokio::time::Duration::from_millis(RECEIVE_TIMEOUT_MS));
                subscribed.cancellable_receive(buffer, 1, 3, timeout).await
            } else {
                subscribed.receive(buffer, 1, 3).await
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
        let mut cnt = CONSUMER_ITERATIONS;
        println!("[RECEIVER] Stream processor started");
        while cnt > 0 {
            // Use timeout to avoid waiting indefinitely in case of issues with the producer or subscription
            match tokio::time::timeout(
                tokio::time::Duration::from_millis(SAMPLE_INTERVAL_MS),
                stream.next(),
            )
            .await
            {
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
        tokio::time::sleep(tokio::time::Duration::from_millis(
            TIMEOUT_FOR_SIMULATION_MS,
        ))
        .await;
        //Producer create
        let producer_runtime = get_test_runtime();
        let producer = create_producer(producer_runtime, service_id_clone);
        // Spawn async data sender
        let sender_join_handle = tokio::spawn(async_data_sender_fn(producer));
        // Await consumer creation and subscribe to events
        let consumer = consumer.await.expect("Failed to create consumer");
        // Subscribe to one event
        let subscribed = consumer.left_tire.subscribe(SAMPLE_COUNT).unwrap();

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
        tokio::time::sleep(tokio::time::Duration::from_millis(
            TIMEOUT_FOR_SIMULATION_MS,
        ))
        .await;
        //Producer create
        let producer_runtime = get_test_runtime();
        let producer = create_producer(producer_runtime, service_id_clone);
        // Spawn async data sender
        let sender_join_handle = tokio::spawn(async_data_sender_fn(producer));
        // Await consumer creation and subscribe to events
        let consumer = consumer.await.expect("Failed to create consumer");
        // Subscribe to one event
        let subscribed = consumer.left_tire.subscribe(SAMPLE_COUNT).unwrap();

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
        tokio::time::sleep(tokio::time::Duration::from_millis(
            TIMEOUT_FOR_SIMULATION_MS,
        ))
        .await;
        //Producer create
        let producer_runtime = get_test_runtime();
        let producer = create_producer(producer_runtime, service_id_clone);
        // Spawn async data sender
        let sender_join_handle = tokio::spawn(async_data_sender_fn(producer));
        // Await consumer creation and subscribe to events
        let consumer = consumer.await.expect("Failed to create consumer");
        // Subscribe to one event
        let subscribed = consumer.left_tire.subscribe(SAMPLE_COUNT).unwrap();
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
