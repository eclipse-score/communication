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

use crate::VehicleOfferedProducer;
use com_api::{
    Builder, InstanceSpecifier, OfferedProducer, Producer, Publisher, Result, Runtime,
    SampleMaybeUninit, SampleMut,
};
use com_api_gen::{Tire, VehicleInterface};
use std::thread;
use std::time::Duration;

// Delay between sample data sending in the producer loop.
const PRODUCER_SEND_INTERVAL_MS: u64 = 1000;

// Example struct demonstrating composition with VehicleConsumer
pub struct VehicleMonitorProducer<R: Runtime> {
    producer: VehicleOfferedProducer<R>,
}

impl<R: Runtime> VehicleMonitorProducer<R> {
    /// Create a new VehicleMonitorProducer
    pub fn new(runtime: &R, service_id: InstanceSpecifier) -> Result<Self> {
        let producer_builder = runtime.producer_builder::<VehicleInterface>(service_id);
        let producer = producer_builder
            .build()
            .expect("Failed to build producer instance");
        let producer = producer.offer().expect("Failed to offer producer instance");
        Ok(Self { producer })
    }

    /// Publish tire data using the producer
    pub fn publish_tire_data(&self, tire: Tire) -> Result<()> {
        let uninit_sample = self.producer.left_tire.allocate()?;
        let sample = uninit_sample.write(tire);
        sample.send()?;
        println!("Tire data sent");
        Ok(())
    }

    /// Publish tire data in a loop, incrementing pressure each iteration
    pub fn run_publish_loop(&self, initial_pressure: f32, count: usize) {
        for i in 0..count {
            match self.publish_tire_data(Tire {
                pressure: initial_pressure + i as f32,
            }) {
                Ok(_) => (),
                Err(e) => eprintln!("Failed to publish tire data: {:?}", e),
            }
            thread::sleep(Duration::from_millis(PRODUCER_SEND_INTERVAL_MS));
        }
    }

    /// Stop offering the service and release the producer.
    pub fn unoffer(self) {
        match self.producer.unoffer() {
            Ok(_) => println!("Successfully unoffered the service"),
            Err(e) => eprintln!("Failed to unoffer: {:?}", e),
        }
    }
}
