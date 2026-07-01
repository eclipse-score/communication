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
    ConsumerBuilder, FindServiceSpecifier, InstanceSpecifier, Result, Runtime, SampleContainer,
    ServiceDiscovery, Subscriber, Subscription,
};
use com_api_gen::{Exhaust, Tire, VehicleInterface};
use futures::channel::oneshot;
use futures::{FutureExt, StreamExt};
use std::sync::mpsc;
use std::thread;
use std::time::Duration;

/// Timeout used when performing a cancellable async receive.
const RECEIVE_TIMEOUT_MS: u64 = 500;
/// Polling interval between iterations in synchronous receive loops.
const SYNC_RECEIVE_POLL_INTERVAL_MS: u64 = 1000;

pub struct VehicleMonitorConsumer<R: Runtime> {
    tire_subscriber: <<R as Runtime>::Subscriber<Tire> as Subscriber<Tire, R>>::Subscription,
    _exhaust_subscriber:
        <<R as Runtime>::Subscriber<Exhaust> as Subscriber<Exhaust, R>>::Subscription,
}

impl<R: Runtime> VehicleMonitorConsumer<R> {
    /// Shared constructor logic: picks the first available service instance and subscribes.
    fn from_service_instances<B>(instances: impl IntoIterator<Item = B>) -> Result<Self>
    where
        B: ConsumerBuilder<VehicleInterface, R>,
    {
        let handle_index = 0; // or any index you need from vector of instances
        let consumer_builder = instances
            .into_iter()
            .nth(handle_index)
            .expect("Failed to get consumer builder at specified handle index");

        let consumer = consumer_builder
            .build()
            .expect("Failed to build consumer instance");

        let tire_subscriber = consumer.left_tire.subscribe(3)?;
        let exhaust_subscriber = consumer.exhaust.subscribe(3)?;

        Ok(Self {
            tire_subscriber,
            _exhaust_subscriber: exhaust_subscriber,
        })
    }

    fn processing_data<S>(&self, result: Result<usize>, sample_buf: &mut SampleContainer<S>)
    where
        S: std::ops::Deref<Target = Tire>,
    {
        match result {
            Ok(0) => println!("No tire data received"),
            Ok(x) => {
                let sample = sample_buf
                    .pop_front()
                    .expect("Sample buffer pop operation error");
                println!("{x} samples received: sample[0] = {:?}", *sample);
            }
            Err(e) => eprintln!("Error receiving tire data: {:?}", e),
        }
    }

    /// Finds available service instances and constructs a VehicleMonitorConsumer.
    /// It will return immediately regardless of whether any instances are available or not.
    pub fn find_available_instances(runtime: &R, service_id: InstanceSpecifier) -> Result<Self> {
        let consumer_discovery =
            runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
        let instances = consumer_discovery
            .get_available_instances()
            .expect("Failed to get available service instances");
        Self::from_service_instances(instances)
    }

    /// Finds available service instances asynchronously and constructs a VehicleMonitorConsumer.
    /// It will wait for the service availability and return once an instance is found.
    pub async fn find_available_instances_async(
        runtime: &R,
        service_id: InstanceSpecifier,
    ) -> Result<Self> {
        let consumer_discovery =
            runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
        let instances = consumer_discovery
            .get_available_instances_async()
            .await
            .expect("Failed to get available service instances asynchronously");
        Self::from_service_instances(instances)
    }

    /// Reads tire data synchronously, returning a Result with the number of samples received.
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

    /// Reads tire data asynchronously without a timeout, returning a Result with the number of samples received.
    pub async fn read_tire_data_async_without_timeout(&self, count: usize) -> Result<String> {
        println!("Performing asynchronous receive without timeout");
        let mut sample_buf = SampleContainer::new(3);
        for _ in 0..count {
            let (mut buf, result) = self.tire_subscriber.receive(sample_buf, 1, 3).await;
            self.processing_data(result, &mut buf);
            sample_buf = buf;
        }
        Ok("All tire data read asynchronously".to_string())
    }

    /// Reads tire data asynchronously with a timeout, returning a Result with the number of samples received.
    pub async fn read_tire_data_async_with_timeout(&self, count: usize) -> Result<String> {
        println!("Performing asynchronous receive with timeout");
        let mut sample_buf = SampleContainer::new(3);

        // Thread for simulating a timeout mechanism.
        // This is just for demonstration purposes, but in a real application you would likely use a more robust timer mechanism or library.
        let (timer_tx, timer_rx) = mpsc::channel::<oneshot::Sender<()>>();
        thread::spawn(move || {
            while let Ok(reply_tx) = timer_rx.recv() {
                thread::sleep(Duration::from_millis(RECEIVE_TIMEOUT_MS));
                let _ = reply_tx.send(());
            }
        });
        for _ in 0..count {
            // Request a timeout from the shared timer thread.
            let (tx, rx) = oneshot::channel();
            timer_tx
                .send(tx)
                .expect("Timer thread unexpectedly stopped");

            // Map the receiver to resolve to () instead of Result<(), Canceled>
            let timeout_future = rx.map(|_| ());
            let (mut buf, result) = self
                .tire_subscriber
                .cancellable_receive(sample_buf, 1, 3, timeout_future)
                .await;
            self.processing_data(result, &mut buf);
            sample_buf = buf;
        }
        Ok("All tire data read asynchronously with timeout".to_string())
    }

    /// Reads tire data as a stream, returning a Result with the number of samples received.
    pub async fn read_tire_data_stream(&mut self, count: usize) {
        let mut stream = self.tire_subscriber.to_stream();
        for _ in 0..count {
            match stream.next().await {
                Some(Ok(sample)) => println!("Received tire data: {:?}", *sample),
                Some(Err(e)) => eprintln!("Failed to receive tire data: {:?}", e),
                None => {
                    println!("Stream ended");
                    break;
                }
            }
        }
    }

    /// Unsubscribe from all events and release the consumer.
    pub fn unsubscribe(self) {
        let _ = self.tire_subscriber.unsubscribe();
        let _ = self._exhaust_subscriber.unsubscribe();
    }
}
