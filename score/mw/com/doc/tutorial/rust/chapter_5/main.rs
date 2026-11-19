/*******************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * <https://www.apache.org/licenses/LICENSE-2.0>
 *
 * SPDX-License-Identifier: Apache-2.0
 ******************************************************************************/

//! Consumer application of the Vehicle Monitor using the asynchronous API.
//! The futures are driven with `futures::executor::block_on()`, any other executor works as well.

mod consumer;

use clap::Parser;
use consumer::VehicleMonitorConsumer;
use score_com::InstanceSpecifier;
use std::path::PathBuf;
use tutorial_common::{DEFAULT_CONFIG_PATH, SAMPLE_COUNT, SERVICE_INSTANCE_SPECIFIER};

use score_log as log;

/// How the samples are received after the asynchronous service discovery.
#[derive(clap::ValueEnum, Clone, Debug, Default)]
enum ExampleType {
    /// Asynchronous receive without timeout.
    #[default]
    AsyncReceive,
    /// Asynchronous receive with a cancellation timeout.
    AsyncReceiveWithTimeout,
    /// Stream based receive.
    Streaming,
}

#[derive(Parser)]
struct Arguments {
    #[arg(short, long, value_enum, default_value = "async-receive")]
    example_type: ExampleType,

    #[arg(short, long, default_value = DEFAULT_CONFIG_PATH)]
    service_instance_manifest: PathBuf,
}

fn main() {
    tutorial_common::init_logging();
    let args = Arguments::parse();
    let runtime = tutorial_common::create_lola_runtime(&args.service_instance_manifest);

    let service_id = InstanceSpecifier::new(SERVICE_INSTANCE_SPECIFIER).expect("Failed to create InstanceSpecifier");
    // Waits until the producer offers the service.
    let mut consumer_monitor = futures::executor::block_on(VehicleMonitorConsumer::find_available_instances_async(
        &runtime, service_id,
    ))
    .expect("Failed to create consumer");

    match args.example_type {
        ExampleType::AsyncReceive => {
            futures::executor::block_on(consumer_monitor.read_tire_data_async_without_timeout(SAMPLE_COUNT))
                .expect("Failed to read tire data asynchronously");
        },
        ExampleType::AsyncReceiveWithTimeout => {
            futures::executor::block_on(consumer_monitor.read_tire_data_async_with_timeout(SAMPLE_COUNT))
                .expect("Failed to read tire data asynchronously with timeout");
        },
        ExampleType::Streaming => {
            futures::executor::block_on(consumer_monitor.read_tire_data_stream(SAMPLE_COUNT));
        },
    }
    consumer_monitor.unsubscribe();
    log::info!("Consumer operation completed");
}
