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

use com_api::{
    Builder, InstanceSpecifier, Producer, Publisher, Result, Runtime, SampleMaybeUninit, SampleMut,
};
use com_api_gen::{Tire, VehicleInterface};

use crate::vehicle_monitor::VehicleMonitorProducer;

impl<R: Runtime> VehicleMonitorProducer<R> {
    /// Create a new VehicleMonitorProducer
    pub fn new(runtime: &R, service_id: InstanceSpecifier) -> Self {
        let producer_builder = runtime.producer_builder::<VehicleInterface>(service_id);
        let producer = producer_builder
            .build()
            .expect("Failed to build producer instance");
        let producer = producer.offer().expect("Failed to offer producer instance");
        Self { producer }
    }

    /// Write tire data using the producer
    pub fn write_tire_data(&self, tire: Tire) -> Result<()> {
        let uninit_sample = self.producer.left_tire.allocate()?;
        let sample = uninit_sample.write(tire);
        sample.send()?;
        println!("Tire data sent");
        Ok(())
    }
}
