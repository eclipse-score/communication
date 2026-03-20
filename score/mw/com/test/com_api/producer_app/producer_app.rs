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

//! Producer (skeleton) binary for the BigData com-api SCT.
//!
//! # Usage
//!
//! ```
//! bigdata-producer [-n <num_cycles>]
//! ```
//! * `-n <num_cycles>` — number of samples to send before exiting (default: 40).
//!
//! The producer sends incrementing `MapApiLanesStamped` samples until it has sent
//! `num_cycles` samples, then exits with code 0.
//! It sleeps for a short duration between sends.
use std::path::Path;
use std::thread;
use std::time::Duration;

use bigdata_com_api_gen::{BigDataInterface, MapApiLanesStamped};
use clap::Parser;
use com_api::{
    Builder, InstanceSpecifier, LolaRuntimeBuilderImpl, Producer, Publisher, Runtime,
    RuntimeBuilder, SampleMaybeUninit, SampleMut,
};

const CONFIG_PATH: &str = "etc/config.json";
const DEFAULT_CYCLES: u32 = 40;
const SERVICE_OFFER_DELAY_MS: u64 = 2000;
const SEND_INTERVAL_MS: u64 = 100;

#[derive(Parser)]
struct Args {
    /// Number of samples to send before exiting
    #[arg(short = 'n', default_value_t = DEFAULT_CYCLES)]
    num_cycles: u32,
}

fn main() {
    let args = Args::parse();
    let num_cycles = args.num_cycles;

    println!("[bigdata-producer] Starting with num_cycles={}", num_cycles);

    // Initialise the Lola runtime.
    let mut runtime_builder = LolaRuntimeBuilderImpl::new();
    runtime_builder.load_config(Path::new(CONFIG_PATH));
    let runtime = runtime_builder
        .build()
        .expect("Failed to build Lola runtime");

    // Create the producer and offer the service.
    let instance_specifier = InstanceSpecifier::new("/score/cp60/MapApiLanesStamped")
        .expect("Invalid instance specifier");
    // Sleep for a few seconds before offering the service to allow the consumer to start first and demonstrate service discovery retries.
    thread::sleep(Duration::from_millis(SERVICE_OFFER_DELAY_MS));

    let producer_builder = runtime.producer_builder::<BigDataInterface>(instance_specifier);
    let producer = producer_builder.build().expect("Failed to build producer");
    let offered = producer.offer().expect("Failed to offer service");

    println!("[bigdata-producer] Service offered, starting send loop");
    for x in 0..num_cycles {
        let uninit = offered
            .map_api_lanes_stamped_
            .allocate()
            .expect("Failed to allocate sample");
        let mut sample = MapApiLanesStamped::default();
        sample.x = x;
        let ready = uninit.write(sample);
        ready.send().expect("Failed to send sample");
        println!("[bigdata-producer] Sent sample x={}", x);

        thread::sleep(Duration::from_millis(SEND_INTERVAL_MS));
    }
}
