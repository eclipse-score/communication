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
//!
//! # Usage
//!
//! ```
//! bigdata-consumer -n <num_cycles>
//! ```
//!
//! `-n <num_cycles>` — number of `MapApiLanesStamped` samples to receive
//! before exiting (required).
//!
//! The consumer retries service discovery until the producer is available,
//! then subscribes to `map_api_lanes_stamped_` and reads exactly `num_cycles`
//! samples before exiting with code 0.

use bigdata_com_api_gen::BigDataInterface;
use clap::Parser;
use com_api::{
    Builder, FindServiceSpecifier, InstanceSpecifier, LolaRuntimeBuilderImpl, Runtime,
    RuntimeBuilder, SampleContainer, ServiceDiscovery, Subscriber, Subscription,
};
use std::path::Path;
use std::thread;
use std::time::Duration;

const CONFIG_PATH: &str = "etc/config.json";
const SERVICE_DISCOVERY_RETRY_MS: u64 = 500;
const MAX_SAMPLES_PER_CALL: usize = 5;

#[derive(Parser)]
struct Args {
    /// Number of samples to receive before exiting
    #[arg(short = 'n', required = true)]
    num_cycles: usize,
}

fn main() {
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

    // Retry until the producer has offered the service.
    let consumer_builder = loop {
        let instances = discovery
            .get_available_instances()
            .expect("Service discovery failed");
        if let Some(builder) = instances.into_iter().next() {
            break builder;
        }
        println!("[bigdata-consumer] Service not yet available, retrying...");
        thread::sleep(Duration::from_millis(SERVICE_DISCOVERY_RETRY_MS));
    };

    let consumer = consumer_builder.build().expect("Failed to build consumer");

    let subscription = consumer
        .map_api_lanes_stamped_
        .subscribe(MAX_SAMPLES_PER_CALL)
        .expect("Failed to subscribe to map_api_lanes_stamped_");

    println!(
        "[bigdata-consumer] Subscribed, waiting for {} samples",
        num_cycles
    );

    let mut received_total: usize = 0;
    let mut sample_buf = SampleContainer::new(MAX_SAMPLES_PER_CALL);

    while received_total < num_cycles {
        let want = (num_cycles - received_total).min(MAX_SAMPLES_PER_CALL);
        match subscription.try_receive(&mut sample_buf, want) {
            Ok(0) => {
                // No data yet — yield briefly and retry.
                thread::sleep(Duration::from_millis(1));
            }
            Ok(n) => {
                for _ in 0..n {
                    if let Some(sample) = sample_buf.pop_front() {
                        println!("[bigdata-consumer] Received sample x={}", sample.x);
                    }
                }
                received_total += n;
                println!(
                    "[bigdata-consumer] Progress: {}/{}",
                    received_total, num_cycles
                );
            }
            Err(e) => {
                eprintln!("[bigdata-consumer] Receive error: {:?}", e);
                std::process::exit(1);
            }
        }
    }

    println!(
        "[bigdata-consumer] Received all {} samples, exiting",
        num_cycles
    );
}
