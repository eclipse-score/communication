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
    Builder, FindServiceSpecifier, InstanceSpecifier, Result, Runtime, SampleContainer,
    ServiceDiscovery, Subscriber, Subscription,
};
use com_api_gen::VehicleInterface;

use crate::vehicle_monitor::VehicleMonitorConsumer;

impl<R: Runtime> VehicleMonitorConsumer<R> {
    /// Create a new VehicleMonitorConsumer
    pub fn new(runtime: &R, service_id: InstanceSpecifier) -> Self {
        let consumer_discovery =
            runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
        let available_service_instances = consumer_discovery
            .get_available_instances()
            .expect("Failed to get available service instances");

        // Select service instance at specific handle_index
        let handle_index = 0; // or any index you need from vector of instances
        let consumer_builder = available_service_instances
            .into_iter()
            .nth(handle_index)
            .expect("Failed to get consumer builder at specified handle index");

        let consumer = consumer_builder
            .build()
            .expect("Failed to build consumer instance");

        let tire_subscriber = consumer.left_tire.subscribe(3).unwrap();
        let exhaust_subscriber = consumer.exhaust.subscribe(3).unwrap();

        Self {
            tire_subscriber,
            _exhaust_subscriber: exhaust_subscriber,
        }
    }

    /// Monitor tire data from the consumer
    pub fn read_tire_data(&self) -> Result<String> {
        let mut sample_buf = SampleContainer::new(3);

        match self.tire_subscriber.try_receive(&mut sample_buf, 1) {
            Ok(0) => Ok("No tire data received".to_string()),
            Ok(x) => {
                let sample = sample_buf.pop_front().unwrap();
                Ok(format!("{} samples received: sample[0] = {:?}", x, *sample))
            }
            Err(e) => Err(e),
        }
    }
}
