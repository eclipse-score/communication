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

//! bigdata-consumer -n <num_cycles> [-t <test_name>]
//! ```
//!
//! * `-n <num_cycles>` — number of samples to receive before exiting (required).
//! * `-t <test_name>` — which payload type to test: `bigdata` (default),
//!   `mixed_primitives`, or `complex_struct`.
//!
//! The consumer retries service discovery until the producer is available,
//! then subscribes to the selected event and reads exactly `num_cycles`
//! samples before exiting with code 0.

use bigdata_com_api_gen::{
    ArrayStruct, BigDataInterface, ComplexStruct, ComplexStructInterface, MixedPrimitivesInterface,
    MixedPrimitivesPayload, NestedStruct, Point, Point3D, SensorData, SimpleStruct, VehicleState,
};
use clap::Parser;
use com_api::{
    Builder, FindServiceSpecifier, InstanceSpecifier, LolaRuntimeBuilderImpl, Result, Runtime,
    RuntimeBuilder, SampleContainer, ServiceDiscovery, Subscriber, Subscription,
};
use std::path::Path;
use std::thread;
use std::time::Duration;

const CONFIG_PATH: &str = "etc/config.json";
const SERVICE_DISCOVERY_RETRY_MS: Duration = Duration::from_millis(500);
const MAX_SAMPLES_PER_CALL: usize = 5;

#[derive(Clone, clap::ValueEnum)]
enum TestCase {
    Bigdata,
    MixedPrimitives,
    ComplexStruct,
}

#[derive(Parser)]
struct Args {
    /// Number of samples to receive before exiting
    #[arg(short = 'n', required = true)]
    num_cycles: usize,
    /// Which payload type to test
    #[arg(short = 't', long = "test", default_value = "bigdata")]
    test_case: TestCase,
}

/// Helper function to receive samples in a loop until `num_cycles` samples have been received.
fn receive_loop(
    num_cycles: usize,
    tag: &str,
    retry_ms: Duration,
    mut try_receive: impl FnMut(usize) -> Result<usize>,
) {
    let mut received_total: usize = 0;
    while received_total < num_cycles {
        let want = (num_cycles - received_total).min(MAX_SAMPLES_PER_CALL);
        match try_receive(want) {
            Ok(0) => thread::sleep(retry_ms),
            Ok(n) => {
                received_total += n;
                println!("{} Progress: {}/{}", tag, received_total, num_cycles);
            }
            Err(e) => {
                eprintln!("{} Receive error: {:?}", tag, e);
                std::process::exit(1);
            }
        }
    }
    println!("{} Received all {} samples, exiting", tag, num_cycles);
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
        // The bigdata test uses the `BigDataInterface` and its `map_api_lanes_stamped_` event.
        TestCase::Bigdata => {
            println!(
                "[bigdata-consumer] Starting bigdata test, will receive {} samples",
                num_cycles
            );
            let instance_specifier = InstanceSpecifier::new("/score/cp60/MapApiLanesStamped")
                .expect("Invalid instance specifier");
            let discovery = runtime.find_service::<BigDataInterface>(
                FindServiceSpecifier::Specific(instance_specifier),
            );

            let consumer_builder = loop {
                let instances = discovery
                    .get_available_instances()
                    .expect("Service discovery failed");
                if let Some(builder) = instances.into_iter().next() {
                    break builder;
                }
                println!("[bigdata-consumer] Service not yet available, retrying...");
                thread::sleep(SERVICE_DISCOVERY_RETRY_MS);
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

            let mut sample_buf = SampleContainer::new(MAX_SAMPLES_PER_CALL);
            receive_loop(num_cycles, "[bigdata-consumer]", Duration::from_millis(100), |max| {
                let n = subscription.try_receive(&mut sample_buf, max)?;
                for _ in 0..n {
                    if let Some(sample) = sample_buf.pop_front() {
                        println!("[bigdata-consumer] Received sample x={}", sample.x);
                    }
                }
                Ok(n)
            });
        }
        // The mixed_primitives test uses the `MixedPrimitivesInterface` and its `mixed_event` event.
        TestCase::MixedPrimitives => {
            println!(
                "[mixed-primitives-consumer] Starting mixed_primitives test, will receive {} samples",
                num_cycles
            );
            let instance_specifier = InstanceSpecifier::new("/IntegrationTest/MixedPrimitives")
                .expect("Invalid instance specifier");
            let discovery = runtime.find_service::<MixedPrimitivesInterface>(
                FindServiceSpecifier::Specific(instance_specifier),
            );

            let consumer_builder = loop {
                let instances = discovery
                    .get_available_instances()
                    .expect("Service discovery failed");
                if let Some(builder) = instances.into_iter().next() {
                    break builder;
                }
                println!("[mixed-primitives-consumer] Service not yet available, retrying...");
                thread::sleep(SERVICE_DISCOVERY_RETRY_MS);
            };

            let consumer = consumer_builder.build().expect("Failed to build consumer");
            let subscription = consumer
                .mixed_event
                .subscribe(MAX_SAMPLES_PER_CALL)
                .expect("Failed to subscribe to mixed_event");

            println!(
                "[mixed-primitives-consumer] Subscribed, waiting for {} samples",
                num_cycles
            );

            let mut sample_buf = SampleContainer::new(MAX_SAMPLES_PER_CALL);
            receive_loop(num_cycles, "[mixed-primitives-consumer]", Duration::from_millis(100), |max| {
                let n = subscription.try_receive(&mut sample_buf, max)?;

                for _ in 0..n {
                    if let Some(sample) = sample_buf.pop_front() {
                        let x = sample.u64_val;
                        let expected = MixedPrimitivesPayload {
                            u64_val: x,
                            i64_val: i64::try_from(x).expect("Failed to convert x to i64"),
                            u32_val: u32::try_from(x).expect("Failed to convert x to u32"),
                            i32_val: i32::try_from(x).expect("Failed to convert x to i32"),
                            f32_val: x as f32 / 2.0,
                            u16_val: u16::try_from(x).expect("Failed to convert x to u16"),
                            i16_val: i16::try_from(x).expect("Failed to convert x to i16"),
                            u8_val: u8::try_from(x).expect("Failed to convert x to u8"),
                            i8_val: i8::try_from(x).expect("Failed to convert x to i8"),
                            flag: x % 2 == 0,
                        };
                        assert_eq!(sample.u64_val, expected.u64_val);
                        assert_eq!(sample.i64_val, expected.i64_val);
                        assert_eq!(sample.u32_val, expected.u32_val);
                        assert_eq!(sample.i32_val, expected.i32_val);
                        assert!((sample.f32_val - expected.f32_val).abs() < 0.01_f32);
                        assert_eq!(sample.u16_val, expected.u16_val);
                        assert_eq!(sample.i16_val, expected.i16_val);
                        assert_eq!(sample.u8_val, expected.u8_val);
                        assert_eq!(sample.i8_val, expected.i8_val);
                        assert_eq!(sample.flag, expected.flag);
                        println!(
                            "[mixed-primitives-consumer] Received sample u64={} i64={} u32={} i32={} f32={} u16={} i16={} u8={} i8={} flag={}",
                            sample.u64_val, sample.i64_val, sample.u32_val,
                            sample.i32_val, sample.f32_val, sample.u16_val,
                            sample.i16_val, sample.u8_val, sample.i8_val, sample.flag
                        );
                    }
                }
                Ok(n)
            });
        }
        // The complex_struct test uses the `ComplexStructInterface` and its `complex_event` event.
        TestCase::ComplexStruct => {
            println!(
                "[complex-struct-consumer] Starting complex_struct test, will receive {} samples",
                num_cycles
            );
            let instance_specifier = InstanceSpecifier::new("/UserDefinedTest/ComplexStruct")
                .expect("Invalid instance specifier");
            let discovery = runtime.find_service::<ComplexStructInterface>(
                FindServiceSpecifier::Specific(instance_specifier),
            );

            let consumer_builder = loop {
                let instances = discovery
                    .get_available_instances()
                    .expect("Service discovery failed");
                if let Some(builder) = instances.into_iter().next() {
                    break builder;
                }
                println!("[complex-struct-consumer] Service not yet available, retrying...");
                thread::sleep(SERVICE_DISCOVERY_RETRY_MS);
            };

            let consumer = consumer_builder.build().expect("Failed to build consumer");
            let subscription = consumer
                .complex_event
                .subscribe(MAX_SAMPLES_PER_CALL)
                .expect("Failed to subscribe to complex_event");

            println!(
                "[complex-struct-consumer] Subscribed, waiting for {} samples",
                num_cycles
            );

            let mut sample_buf = SampleContainer::new(MAX_SAMPLES_PER_CALL);
            receive_loop(num_cycles, "[complex-struct-consumer]", Duration::from_millis(100), |max| {
                let n = subscription.try_receive(&mut sample_buf, max)?;
                for _ in 0..n {
                    if let Some(sample) = sample_buf.pop_front() {
                        let x_u32 = sample.count;
                        let x_f32 = x_u32 as f32;
                        let expected = ComplexStruct {
                            count: x_u32,
                            simple: SimpleStruct { id: x_u32 },
                            nested: NestedStruct {
                                id: x_u32,
                                simple: SimpleStruct { id: x_u32 },
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
                                sensor_id: u16::try_from(x_u32)
                                    .expect("Failed to convert x_u32 to u16"),
                                temperature: x_f32 / 2.0,
                                humidity: x_f32 / 2.0,
                                pressure: x_f32 / 2.0,
                            },
                            vehicle: VehicleState {
                                speed: x_f32 / 2.0,
                                rpm: u16::try_from(x_u32).expect("Failed to convert x_u32 to u16"),
                                fuel_level: x_f32 / 2.0,
                                is_running: x_u32 % 2 == 0,
                                mileage: x_u32,
                            },
                            array: ArrayStruct { values: [x_u32; 5] },
                        };
                        assert_eq!(sample.count, expected.count);
                        assert_eq!(sample.simple.id, expected.simple.id);
                        assert_eq!(sample.nested.id, expected.nested.id);
                        assert_eq!(sample.nested.simple.id, expected.nested.simple.id);
                        assert!((sample.nested.value - expected.nested.value).abs() < 0.01_f32);
                        assert!((sample.point.x - expected.point.x).abs() < 0.001_f32);
                        assert!((sample.point.y - expected.point.y).abs() < 0.001_f32);
                        assert!((sample.point3d.x - expected.point3d.x).abs() < 0.001_f32);
                        assert!((sample.point3d.y - expected.point3d.y).abs() < 0.001_f32);
                        assert!((sample.point3d.z - expected.point3d.z).abs() < 0.001_f32);
                        assert_eq!(sample.sensor.sensor_id, expected.sensor.sensor_id);
                        assert!(
                            (sample.sensor.temperature - expected.sensor.temperature).abs()
                                < 0.01_f32
                        );
                        assert!(
                            (sample.sensor.humidity - expected.sensor.humidity).abs() < 0.01_f32
                        );
                        assert!(
                            (sample.sensor.pressure - expected.sensor.pressure).abs() < 0.01_f32
                        );
                        assert!((sample.vehicle.speed - expected.vehicle.speed).abs() < 0.1_f32);
                        assert_eq!(sample.vehicle.rpm, expected.vehicle.rpm);
                        assert!(
                            (sample.vehicle.fuel_level - expected.vehicle.fuel_level).abs()
                                < 0.1_f32
                        );
                        assert_eq!(sample.vehicle.is_running, expected.vehicle.is_running);
                        assert_eq!(sample.vehicle.mileage, expected.vehicle.mileage);
                        for i in 0..sample.array.values.len() {
                            assert_eq!(
                                sample.array.values[i], expected.array.values[i],
                                "array.values[{}] mismatch",
                                i
                            );
                        }
                        println!(
                            "[complex-struct-consumer] Received sample count={} point=({},{}) speed={}",
                            sample.count, sample.point.x, sample.point.y,
                            sample.vehicle.speed
                        );
                    }
                }
                Ok(n)
            });
        }
    }
}
