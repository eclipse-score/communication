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

use com_api_gen::{Exhaust, Tire, VehicleInterface};
use score_com::{
    ConsumerBuilder, FindServiceSpecifier, InstanceSpecifier, Result, Runtime, SampleContainer, ServiceDiscovery,
    Subscriber, Subscription,
};
use std::thread;
use std::time::Duration;

use score_log as log;

/// Polling interval between iterations in synchronous receive loops.
const SYNC_RECEIVE_POLL_INTERVAL_MS: u64 = 1000;

pub struct VehicleMonitorConsumer<R: Runtime> {
    tire_subscriber: <<R as Runtime>::Subscriber<Tire> as Subscriber<Tire, R>>::Subscription,
    _exhaust_subscriber: <<R as Runtime>::Subscriber<Exhaust> as Subscriber<Exhaust, R>>::Subscription,
}

impl<R: Runtime> VehicleMonitorConsumer<R> {
    // [rust-consumer-subscribe-start]
    /// Picks the first available service instance and subscribes to its events.
    fn from_service_instances<B>(instances: impl IntoIterator<Item = B>) -> Result<Self>
    where
        B: ConsumerBuilder<VehicleInterface, R>,
    {
        let handle_index = 0; // or any index you need from vector of instances
        let consumer_builder = instances
            .into_iter()
            .nth(handle_index)
            .expect("Failed to get consumer builder at specified handle index");

        let consumer = consumer_builder.build()?;

        let tire_subscriber = consumer.left_tire.subscribe(3)?;
        let exhaust_subscriber = consumer.exhaust.subscribe(3)?;

        Ok(Self {
            tire_subscriber,
            _exhaust_subscriber: exhaust_subscriber,
        })
    }
    // [rust-consumer-subscribe-end]

    // [rust-consumer-discovery-start]
    /// Finds available service instances and constructs a VehicleMonitorConsumer.
    /// It will return immediately regardless of whether any instances are available or not.
    pub fn find_available_instances(runtime: &R, service_id: InstanceSpecifier) -> Result<Self> {
        let consumer_discovery = runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
        let instances = consumer_discovery.get_available_instances()?;
        Self::from_service_instances(instances)
    }
    // [rust-consumer-discovery-end]

    fn processing_data<S>(&self, result: Result<usize>, sample_buf: &mut SampleContainer<S>)
    where
        S: std::ops::Deref<Target = Tire>,
    {
        match result {
            Ok(0) => log::info!("No tire data received"),
            Ok(x) => {
                let sample = sample_buf.pop_front().expect("Sample buffer pop operation error");
                log::info!("{} samples received: sample[0] = {:?}", x, *sample);
            },
            Err(e) => log::error!("Error receiving tire data: {:?}", e),
        }
    }

    // [rust-consumer-sync-receive-start]
    /// Reads tire data synchronously, polling `count` times.
    pub fn read_tire_data(&self, count: usize) -> Result<String> {
        let mut sample_buf = SampleContainer::new(3);
        for _ in 0..count {
            let ret = self.tire_subscriber.try_receive(&mut sample_buf, 1);
            self.processing_data(ret, &mut sample_buf);
            // Sleep for a while before the next synchronous receive attempt
            thread::sleep(Duration::from_millis(SYNC_RECEIVE_POLL_INTERVAL_MS));
        }
        Ok("All tire data read successfully".to_string())
    }
    // [rust-consumer-sync-receive-end]

    /// Unsubscribe from all events and release the consumer.
    pub fn unsubscribe(self) {
        let _ = self.tire_subscriber.unsubscribe();
        let _ = self._exhaust_subscriber.unsubscribe();
    }
}
