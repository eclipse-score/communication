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

use com_api::{Builder, InstanceSpecifier, LolaRuntimeBuilderImpl, Runtime, RuntimeBuilder};

use com_api_example::{ExampleType, VehicleMonitorConsumer, VehicleMonitorProducer};

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
    println!("\n=== Running with {name} runtime ===");
    let service_id = InstanceSpecifier::new("/Vehicle/Service1/Instance")
        .expect("Failed to create InstanceSpecifier");
    if matches!(example_type, ExampleType::Sync) {
        std::thread::sleep(std::time::Duration::from_millis(
            SYNC_CONSUMER_STARTUP_DELAY_MS,
        ));
    }
    let mut consumer_monitor = futures::executor::block_on(VehicleMonitorConsumer::new(
        runtime,
        service_id,
        example_type,
    ))
    .expect("Failed to create consumer");
    futures::executor::block_on(consumer_monitor.run_receive(example_type, SAMPLE_COUNT));
    consumer_monitor.unsubscribe();
    println!("=== {name} runtime completed ===\n");
}

// Run the producer with the specified runtime and example type
fn run_producer<R: Runtime>(name: &str, example_type: &ExampleType, runtime: &R) {
    println!("\n=== Running with {name} runtime ===");
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
    println!("Producer created and offered successfully");
    producer_monitor.run_publish_loop(INITIAL_TIRE_PRESSURE, SAMPLE_COUNT);
    producer_monitor.unoffer();
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

fn main() {
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

    // Spawn producer thread
    let producer_handle = match args.run_mode {
        RunMode::Both | RunMode::Producer => {
            let producer_example_type = example_type.clone();
            Some(thread::spawn(move || {
                run_producer("Lola", &producer_example_type, runtime_producer.as_ref());
            }))
        }
        RunMode::Consumer => None,
    };

    // Spawn consumer thread
    let consumer_handle = match args.run_mode {
        RunMode::Both | RunMode::Consumer => Some(thread::spawn(move || {
            run_consumer("Lola", &example_type, runtime_consumer.as_ref());
        })),
        RunMode::Producer => None,
    };

    // Wait for both threads to complete
    if let Some(h) = producer_handle {
        h.join().expect("Producer thread panicked");
    }
    if let Some(h) = consumer_handle {
        h.join().expect("Consumer thread panicked");
    }
}
