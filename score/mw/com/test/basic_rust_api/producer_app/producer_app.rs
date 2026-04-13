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
//! bigdata-producer [-n <num_cycles>] [--test <test_name>]
//! ```
//! * `-n <num_cycles>` — number of samples to send before exiting (default: 40).
//! * `--test <test_name>` — which payload type to test: `bigdata` (default),
//!   `mixed_primitives`, or `complex_struct`.
//!
//! The producer sends incrementing samples until it has sent `num_cycles` samples,
//! then exits with code 0.
//! It sleeps for a short duration between sends.
use std::path::Path;
use std::thread;
use std::time::Duration;

use bigdata_com_api_gen::{
    ArrayStruct, BigDataInterface, ComplexStruct, ComplexStructInterface, MapApiLanesStamped,
    MixedPrimitivesInterface, MixedPrimitivesPayload, NestedStruct, Point, Point3D, SensorData,
    SimpleStruct, VehicleState,
};
use clap::Parser;
use com_api::{
    Builder, InstanceSpecifier, LolaRuntimeBuilderImpl, Producer, Publisher, Runtime,
    RuntimeBuilder, SampleMaybeUninit, SampleMut,
};

const CONFIG_PATH: &str = "etc/config.json";
const DEFAULT_CYCLES: u32 = 40;
const SERVICE_OFFER_DELAY_MS: Duration = Duration::from_millis(2000);
const SEND_INTERVAL_MS: Duration = Duration::from_millis(100);

#[derive(Clone, clap::ValueEnum)]
enum TestCase {
    Bigdata,
    MixedPrimitives,
    ComplexStruct,
}

#[derive(Parser)]
struct Args {
    /// Number of samples to send before exiting
    #[arg(short = 'n', default_value_t = DEFAULT_CYCLES)]
    num_cycles: u32,
    /// Which payload type to test
    #[arg(short = 't', long = "test", default_value = "bigdata")]
    test_case: TestCase,
}

fn run_send_loop(num_cycles: u32, mut send_one: impl FnMut(u32)) {
    for x in 1..num_cycles {
        send_one(x);
        thread::sleep(SEND_INTERVAL_MS);
    }
}

fn run_bigdata_test<R: Runtime>(runtime: &R, num_cycles: u32) {
    println!(
        "[bigdata-producer] Starting bigdata test with num_cycles={}",
        num_cycles
    );
    let instance_specifier = InstanceSpecifier::new("/score/cp60/MapApiLanesStamped")
        .expect("Invalid instance specifier");
    // Sleep to allow the consumer to start first and demonstrate service discovery retries.
    thread::sleep(SERVICE_OFFER_DELAY_MS);

    let producer_builder = runtime.producer_builder::<BigDataInterface>(instance_specifier);
    let producer = producer_builder.build().expect("Failed to build producer");
    let offered = producer.offer().expect("Failed to offer service");

    println!("[bigdata-producer] Service offered, starting send loop");
    run_send_loop(num_cycles, |x| {
        let uninit = offered
            .map_api_lanes_stamped_
            .allocate()
            .expect("Failed to allocate sample");
        let mut sample = MapApiLanesStamped::default();
        sample.x = x;
        let ready = uninit.write(sample);
        ready.send().expect("Failed to send sample");
        println!("[bigdata-producer] Sent sample x={}", x);
    });
}

fn run_mixed_primitives_test<R: Runtime>(runtime: &R, num_cycles: u32) {
    println!(
        "[mixed-primitives-producer] Starting mixed_primitives test with num_cycles={}",
        num_cycles
    );
    let instance_specifier = InstanceSpecifier::new("/IntegrationTest/MixedPrimitives")
        .expect("Invalid instance specifier");
    // Sleep to allow the consumer to start first and demonstrate service discovery retries.
    thread::sleep(SERVICE_OFFER_DELAY_MS);

    let producer_builder = runtime.producer_builder::<MixedPrimitivesInterface>(instance_specifier);
    let producer = producer_builder.build().expect("Failed to build producer");
    let offered = producer.offer().expect("Failed to offer service");

    println!("[mixed-primitives-producer] Service offered, starting send loop");
    run_send_loop(num_cycles, |x| {
        let uninit = offered
            .mixed_event
            .allocate()
            .expect("Failed to allocate sample");
        let sample = MixedPrimitivesPayload {
            u64_val: u64::from(x),
            i64_val: i64::from(x),
            u32_val: x,
            i32_val: i32::try_from(x).expect("Failed to convert x to i32"),
            f32_val: x as f32 / 2.0, // no stdlib alternative
            u16_val: u16::try_from(x).expect("Failed to convert x to u16"),
            i16_val: i16::try_from(x).expect("Failed to convert x to i16"),
            u8_val: u8::try_from(x).expect("Failed to convert x to u8"),
            i8_val: i8::try_from(x).expect("Failed to convert x to i8"),
            flag: x % 2 == 0,
        };
        let ready = uninit.write(sample);
        ready.send().expect("Failed to send sample");
        println!("[mixed-primitives-producer] Sent sample u32_val={}", x);
    });
}

fn run_complex_struct_test<R: Runtime>(runtime: &R, num_cycles: u32) {
    println!(
        "[complex-struct-producer] Starting complex_struct test with num_cycles={}",
        num_cycles
    );
    let instance_specifier = InstanceSpecifier::new("/UserDefinedTest/ComplexStruct")
        .expect("Invalid instance specifier");
    // Sleep to allow the consumer to start first and demonstrate service discovery retries.
    thread::sleep(SERVICE_OFFER_DELAY_MS);

    let producer_builder = runtime.producer_builder::<ComplexStructInterface>(instance_specifier);
    let producer = producer_builder.build().expect("Failed to build producer");
    let offered = producer.offer().expect("Failed to offer service");

    println!("[complex-struct-producer] Service offered, starting send loop");
    run_send_loop(num_cycles, |x| {
        let uninit = offered
            .complex_event
            .allocate()
            .expect("Failed to allocate sample");
        let x_f32 = x as f32;
        let sample = ComplexStruct {
            count: x,
            simple: SimpleStruct { id: x },
            nested: NestedStruct {
                id: x,
                simple: SimpleStruct { id: x },
                value: x_f32 / 5.0,
            },
            point: Point {
                x: x_f32 / 2.0,
                y: x_f32 / 2.0,
            },
            point3d: Point3D {
                x: x_f32 / 3.0,
                y: x_f32 / 3.0,
                z: x_f32 / 3.0,
            },
            sensor: SensorData {
                sensor_id: u16::try_from(x).expect("Failed to convert x to u16"),
                temperature: x_f32 / 2.0,
                humidity: x_f32 / 2.0,
                pressure: x_f32 / 2.0,
            },
            vehicle: VehicleState {
                speed: x_f32 / 2.0,
                rpm: u16::try_from(x).expect("Failed to convert x to u16"),
                fuel_level: x_f32 / 2.0,
                is_running: x % 2 == 0,
                mileage: x,
            },
            array: ArrayStruct { values: [x; 5] },
        };
        let ready = uninit.write(sample);
        ready.send().expect("Failed to send sample");
        println!("[complex-struct-producer] Sent sample count={}", x);
    });
}

fn main() {
    let args = Args::parse();
    let num_cycles = args.num_cycles;

    // Initialise the Lola runtime.
    let mut runtime_builder = LolaRuntimeBuilderImpl::new();
    runtime_builder.load_config(Path::new(CONFIG_PATH));
    let runtime = runtime_builder
        .build()
        .expect("Failed to build Lola runtime");

    match args.test_case {
        TestCase::Bigdata => run_bigdata_test(&runtime, num_cycles),
        TestCase::MixedPrimitives => run_mixed_primitives_test(&runtime, num_cycles),
        TestCase::ComplexStruct => run_complex_struct_test(&runtime, num_cycles),
    }
}
