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
// Note: The example is may using unwrap and panic in some places for simplicity,
// but it is recommended to handle errors properly in production code.

use clap::Parser;
use std::path::PathBuf;
use std::sync::Arc;
use std::thread;

use score_log as log;
use stdout_logger::StdoutLoggerBuilder;

use com_api::{Builder, InstanceSpecifier, LolaRuntimeBuilderImpl, Runtime, RuntimeBuilder};

use com_api_example::{VehicleMonitorConsumer, VehicleMonitorProducer};

/// Controls which roles are started in this process.
#[derive(clap::ValueEnum, Clone, Debug, Default)]
enum RunMode {
    /// Start both the producer and consumer (default).
    #[default]
    Both,
    /// Start only the producer.
    Producer,
    /// Start only the consumer.
    Consumer,
}

/// All execution modes supported by the example application.
/// Used by clap for CLI argument parsing and by the consumer/producer to select behaviour.
#[derive(clap::ValueEnum, Clone, Debug)]
pub enum ExampleType {
    /// Synchronous service discovery and synchronous receive.
    Sync,
    /// Asynchronous service discovery, then synchronous receive.
    AsyncServiceDiscovery,
    /// Asynchronous service discovery and asynchronous receive (no timeout).
    AsyncReceive,
    /// Asynchronous service discovery and asynchronous receive with a cancellation timeout.
    AsyncReceiveWithTimeout,
    /// Asynchronous service discovery and stream-based receive.
    Streaming,
}

#[derive(Parser)]
struct Arguments {
    #[arg(short, long, value_enum, default_value = "both")]
    run_mode: RunMode,

    #[arg(short, long, value_enum, default_value = "sync")]
    example_type: ExampleType,

    #[arg(
        short,
        long,
        default_value = "./score/mw/com/example/com-api-example/etc/mw_com_config.json"
    )]
    service_instance_manifest: PathBuf,
}

/// Number of samples to publish / receive in each example run.
const SAMPLE_COUNT: usize = 5;
/// Starting tire pressure value for the producer publish loop.
const INITIAL_TIRE_PRESSURE: f32 = 5.0;
/// Milliseconds the sync consumer waits before starting service discovery.
const SYNC_CONSUMER_STARTUP_DELAY_MS: u64 = 500;
/// Milliseconds the producer waits in async examples to let the consumer connect first.
const ASYNC_PRODUCER_STARTUP_DELAY_MS: u64 = 2000;

// Run the consumer with the specified runtime and example type
fn run_consumer<R: Runtime>(name: &str, example_type: &ExampleType, runtime: &R) {
    log::info!("Running with {} runtime", name);
    let service_id = InstanceSpecifier::new("/Vehicle/Service1/Instance")
        .expect("Failed to create InstanceSpecifier");
    if matches!(example_type, ExampleType::Sync) {
        std::thread::sleep(std::time::Duration::from_millis(
            SYNC_CONSUMER_STARTUP_DELAY_MS,
        ));
    }

    let consumer_monitor = match example_type {
        // it does the service discovery and receving data synchronously
        ExampleType::Sync => {
            let consumer_monitor =
                VehicleMonitorConsumer::find_available_instances(runtime, service_id)
                    .expect("Failed to create consumer");
            consumer_monitor
                .read_tire_data(SAMPLE_COUNT)
                .expect("Failed to read tire data synchronously");
            consumer_monitor
        }
        // it does the service discovery asynchronously and receving data synchronously
        ExampleType::AsyncServiceDiscovery => {
            let consumer_monitor = futures::executor::block_on(
                VehicleMonitorConsumer::find_available_instances_async(runtime, service_id),
            )
            .expect("Failed to create consumer");
            consumer_monitor
                .read_tire_data(SAMPLE_COUNT)
                .expect("Failed to read tire data synchronously");
            consumer_monitor
        }
        // it does the service discovery asynchronously and receving data asynchronously without timeout
        // for the receive operation, it will wait until the data is received.
        ExampleType::AsyncReceive => {
            let consumer_monitor = futures::executor::block_on(
                VehicleMonitorConsumer::find_available_instances_async(runtime, service_id),
            )
            .expect("Failed to create consumer");
            futures::executor::block_on(
                consumer_monitor.read_tire_data_async_without_timeout(SAMPLE_COUNT),
            )
            .expect("Failed to read tire data asynchronously");
            consumer_monitor
        }
        // it does the service discovery asynchronously and receving data asynchronously with timeout
        // for the receive operation, it will wait until the data is received or timeout occurs.
        ExampleType::AsyncReceiveWithTimeout => {
            let consumer_monitor = futures::executor::block_on(
                VehicleMonitorConsumer::find_available_instances_async(runtime, service_id),
            )
            .expect("Failed to create consumer");
            futures::executor::block_on(
                consumer_monitor.read_tire_data_async_with_timeout(SAMPLE_COUNT),
            )
            .expect("Failed to read tire data asynchronously with timeout");
            consumer_monitor
        }
        // it does the service discovery asynchronously and receving data asynchronously with streaming
        ExampleType::Streaming => {
            let mut consumer_monitor = futures::executor::block_on(
                VehicleMonitorConsumer::find_available_instances_async(runtime, service_id),
            )
            .expect("Failed to create consumer");
            futures::executor::block_on(consumer_monitor.read_tire_data_stream(SAMPLE_COUNT));
            consumer_monitor
        }
    };
    consumer_monitor.unsubscribe();
    log::info!("runtime execution completed");
}

// Run the producer with the specified runtime and example type
fn run_producer<R: Runtime>(name: &str, example_type: &ExampleType, runtime: &R) {
    log::info!("Running with {} runtime", name);
    let service_id = InstanceSpecifier::new("/Vehicle/Service1/Instance")
        .expect("Failed to create InstanceSpecifier");
    // In async examples, we will not wait before offering the service to simulate async discovery.
    if !matches!(example_type, ExampleType::Sync) {
        std::thread::sleep(std::time::Duration::from_millis(
            ASYNC_PRODUCER_STARTUP_DELAY_MS,
        ));
    }
    let producer_monitor =
        VehicleMonitorProducer::new(runtime, service_id).expect("Failed to create producer");
    log::info!("Producer created and offered successfully");
    producer_monitor.run_publish_loop(INITIAL_TIRE_PRESSURE, SAMPLE_COUNT);
    producer_monitor.unoffer();
    println!("=== Producer Operation Completed ===\n");
}

// Create and configure a Lola runtime builder from the given config path.
fn create_lola_runtime_builder(config_path: &std::path::Path) -> LolaRuntimeBuilderImpl {
    assert!(
        config_path.exists(),
        "Config file not found: {}. Provide a valid path via --service-instance-manifest.",
        config_path.display()
    );
    let mut lola_runtime_builder = LolaRuntimeBuilderImpl::new();
    lola_runtime_builder.load_config(config_path);
    lola_runtime_builder
}

fn init_logging() {
    StdoutLoggerBuilder::new()
        .show_module(false)
        .show_file(true)
        .show_line(false)
        .set_as_default_logger();
}

fn main() {
    init_logging();
    let args = Arguments::parse();
    let lola_runtime_builder = create_lola_runtime_builder(&args.service_instance_manifest);
    let lola_runtime = Arc::new(
        lola_runtime_builder
            .build()
            .expect("Failed to build Lola runtime"),
    );

    let example_type = args.example_type.clone();
    let runtime_producer = Arc::clone(&lola_runtime);
    let runtime_consumer = Arc::clone(&lola_runtime);
    let producer_example_type = example_type.clone();

    let spawn_producer = matches!(args.run_mode, RunMode::Both | RunMode::Producer);
    let spawn_consumer = matches!(args.run_mode, RunMode::Both | RunMode::Consumer);

    let producer_handle = spawn_producer.then(|| {
        thread::spawn(move || {
            run_producer("Lola", &producer_example_type, runtime_producer.as_ref());
        })
    });

    let consumer_handle = spawn_consumer.then(|| {
        thread::spawn(move || {
            run_consumer("Lola", &example_type, runtime_consumer.as_ref());
        })
    });

    for handle in [producer_handle, consumer_handle].into_iter().flatten() {
        handle.join().expect("Thread panicked");
    }
}
