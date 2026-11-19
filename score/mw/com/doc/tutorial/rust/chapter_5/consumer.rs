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

//! Consumer using the asynchronous parts of the API: async discovery, async receive,
//! cancellable receive and streams.

use com_api_gen::{Exhaust, Tire, VehicleInterface};
use futures::channel::oneshot;
use futures::{FutureExt, StreamExt};
use score_com::{
    ConsumerBuilder, FindServiceSpecifier, InstanceSpecifier, Result, Runtime, SampleContainer, ServiceDiscovery,
    Subscriber, Subscription,
};
use std::sync::mpsc;
use std::thread;
use std::time::Duration;

use score_log as log;

/// Timeout used when performing a cancellable async receive.
const RECEIVE_TIMEOUT_MS: u64 = 500;

pub struct VehicleMonitorConsumer<R: Runtime> {
    tire_subscriber: <<R as Runtime>::Subscriber<Tire> as Subscriber<Tire, R>>::Subscription,
    _exhaust_subscriber: <<R as Runtime>::Subscriber<Exhaust> as Subscriber<Exhaust, R>>::Subscription,
}

impl<R: Runtime> VehicleMonitorConsumer<R> {
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

    // [rust-consumer-async-discovery-start]
    /// Finds available service instances asynchronously and constructs a VehicleMonitorConsumer.
    /// It will wait for the service availability and return once an instance is found.
    pub async fn find_available_instances_async(runtime: &R, service_id: InstanceSpecifier) -> Result<Self> {
        let consumer_discovery = runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
        let instances = consumer_discovery.get_available_instances_async().await?;
        Self::from_service_instances(instances)
    }
    // [rust-consumer-async-discovery-end]

    // [rust-consumer-async-receive-start]
    /// Reads tire data asynchronously without a timeout.
    pub async fn read_tire_data_async_without_timeout(&self, count: usize) -> Result<String> {
        log::info!("Performing asynchronous receive without timeout");
        let mut sample_buf = SampleContainer::new(3);
        for _ in 0..count {
            let (mut buf, result) = self.tire_subscriber.receive(sample_buf, 1, 3).await;
            self.processing_data(result, &mut buf);
            sample_buf = buf;
        }
        Ok("All tire data read asynchronously".to_string())
    }
    // [rust-consumer-async-receive-end]

    // [rust-consumer-cancellable-receive-start]
    /// Reads tire data asynchronously with a timeout.
    pub async fn read_tire_data_async_with_timeout(&self, count: usize) -> Result<String> {
        log::info!("Performing asynchronous receive with timeout");
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
            timer_tx.send(tx).expect("Timer thread unexpectedly stopped");

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
    // [rust-consumer-cancellable-receive-end]

    // [rust-consumer-stream-start]
    /// Reads tire data as a stream.
    pub async fn read_tire_data_stream(&mut self, count: usize) {
        let mut stream = self.tire_subscriber.to_stream();
        for _ in 0..count {
            match stream.next().await {
                Some(Ok(sample)) => log::info!("Received tire data: {:?}", *sample),
                Some(Err(e)) => log::error!("Failed to receive tire data: {:?}", e),
                None => {
                    log::info!("Stream ended");
                    break;
                },
            }
        }
    }
    // [rust-consumer-stream-end]

    /// Unsubscribe from all events and release the consumer.
    pub fn unsubscribe(self) {
        let _ = self.tire_subscriber.unsubscribe();
        let _ = self._exhaust_subscriber.unsubscribe();
    }
}
