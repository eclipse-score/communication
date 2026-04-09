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

//! Consumer (proxy) binary for the BigData com-api SCT.
//! it use the async APIs for Service Discovery and Receive samples.
//!
//! # Usage
//!
//! ```
//! bigdata-consumer -n <num_cycles>
//! ```
//! `-n <num_cycles>` — number of samples to receive before exiting.
//! The consumer subscribes to the producer's `map_api_lanes_stamped_` output and
//! prints the `x` field of each received sample until it has received the specified number of samples, at which point it exits.

use bigdata_com_api_gen::BigDataInterface;
use clap::Parser;
use com_api::{
    Builder, FindServiceSpecifier, InstanceSpecifier, LolaRuntimeBuilderImpl, Runtime,
    RuntimeBuilder, SampleContainer, ServiceDiscovery, Subscriber, Subscription,
};
use std::path::Path;

const CONFIG_PATH: &str = "etc/config.json";
const MAX_SAMPLES_PER_CALL: usize = 5;

#[derive(Parser)]
struct Args {
    /// Number of samples to receive before exiting
    #[arg(short = 'n', required = true)]
    num_cycles: usize,
}

fn main() {
    futures::executor::block_on(async_main());
}

async fn async_main() {
    let args = Args::parse();
    let num_cycles = args.num_cycles;

    println!(
        "[bigdata-consumer] Starting, will receive {} samples",
        num_cycles
    );

    // Initialise the Lola runtime.
    let mut runtime_builder = LolaRuntimeBuilderImpl::new();
    runtime_builder.load_config(Path::new(CONFIG_PATH));
    let runtime = runtime_builder
        .build()
        .expect("Failed to build Lola runtime");

    let instance_specifier = InstanceSpecifier::new("/score/cp60/MapApiLanesStamped")
        .expect("Invalid instance specifier");

    let discovery = runtime
        .find_service::<BigDataInterface>(FindServiceSpecifier::Specific(instance_specifier));

    // Await the producer to become available.
    let instances = discovery
        .get_available_instances_async()
        .await
        .expect("Failed to get available instances");

    let builder = instances
        .into_iter()
        .next()
        .expect("No service instances available");
    let consumer = builder.build().expect("Failed to build consumer");

    // Subscribe with a slot buffer large enough for MAX_SAMPLES_PER_CALL.
    let subscription = consumer
        .map_api_lanes_stamped_
        .subscribe(MAX_SAMPLES_PER_CALL)
        .expect("Failed to subscribe to map_api_lanes_stamped_");

    let mut received_total: usize = 0;
    let mut sample_buf = SampleContainer::new(MAX_SAMPLES_PER_CALL);
    // `receive` awaits until at least `min_samples` (1) are available.
    while received_total < num_cycles {
        sample_buf = match subscription
            .receive(sample_buf, 1, MAX_SAMPLES_PER_CALL)
            .await
        {
            Ok(returned_buf) => {
                let count = returned_buf.sample_count();
                if count > 0 {
                    let mut buf = returned_buf;
                    for _ in 0..count {
                        if let Some(sample) = buf.pop_front() {
                            println!("[bigdata-consumer] Received sample x={}", sample.x);
                        }
                    }
                    received_total += count;
                    println!(
                        "[bigdata-consumer] Progress: {}/{}",
                        received_total, num_cycles
                    );
                    buf
                } else {
                    returned_buf
                }
            }
            Err(e) => {
                eprintln!("[bigdata-consumer] Receive error: {:?}", e);
                return;
            }
        };
    }

    println!(
        "[bigdata-consumer] Received all {} samples, exiting",
        num_cycles
    );
}
