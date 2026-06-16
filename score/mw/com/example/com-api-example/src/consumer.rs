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
use com_api_gen::VehicleInterface;
use futures::channel::oneshot;
use futures::{FutureExt, StreamExt};
use std::sync::{mpsc, OnceLock};
use std::thread;
use std::time::Duration;

use crate::vehicle_monitor::VehicleMonitorConsumer;
use crate::ExampleType;

/// Timeout used when performing a cancellable async receive.
const RECEIVE_TIMEOUT_MS: u64 = 500;
/// Polling interval between iterations in synchronous receive loops.
const SYNC_RECEIVE_POLL_INTERVAL_MS: u64 = 1000;

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
            timer_tx: OnceLock::new(),
        })
    }

    /// Create a new VehicleMonitorConsumer.
    ///
    /// Uses synchronous service discovery for `ExampleType::Sync` and
    /// asynchronous service discovery for all other modes.
    pub async fn new(
        runtime: &R,
        service_id: InstanceSpecifier,
        mode: &ExampleType,
    ) -> Result<Self> {
        let consumer_discovery =
            runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
        let instances = match mode {
            ExampleType::Sync => consumer_discovery
                .get_available_instances()
                .expect("Failed to get available service instances"),
            _ => {
                let instances = consumer_discovery
                    .get_available_instances_async()
                    .await
                    .expect("Failed to get available service instances asynchronously");
                println!("Available service instances received asynchronously:");
                instances
            }
        };
        Self::from_service_instances(instances)
    }

    /// Read tire data sample using the strategy defined by `mode`.
    ///
    /// Takes `&self` to support all receive modes including streaming,
    /// which requires exclusive access to the subscriber.
    pub async fn read_tire_data(&self, mode: &ExampleType) -> Result<String> {
        match mode {
            ExampleType::Sync | ExampleType::AsyncServiceDiscovery => {
                let mut sample_buf = SampleContainer::new(3);
                let result = self.tire_subscriber.try_receive(&mut sample_buf, 1);
                match result {
                    Ok(0) => Ok("No tire data received".to_string()),
                    Ok(x) => {
                        let sample = sample_buf.pop_front().unwrap();
                        Ok(format!("{x} samples received: sample[0] = {:?}", *sample))
                    }
                    Err(e) => Err(e),
                }
            }
            ExampleType::AsyncReceive => {
                println!("Performing asynchronous receive without timeout");
                let sample_buf = SampleContainer::new(3);
                let (mut sample_buf, result) = self.tire_subscriber.receive(sample_buf, 1, 3).await;
                match result {
                    Ok(0) => Ok("No tire data received".to_string()),
                    Ok(x) => {
                        let sample = sample_buf.pop_front().unwrap();
                        Ok(format!("{x} samples received: sample[0] = {:?}", *sample))
                    }
                    Err(e) => Err(e),
                }
            }
            ExampleType::AsyncReceiveWithTimeout => {
                println!("Performing asynchronous receive with timeout");
                let sample_buf = SampleContainer::new(3);
                let (tx, rx) = oneshot::channel::<()>();
                // Lazily spawn the timer thread on the first cancellable receive call.
                // It is created only once and exits when the VehicleMonitorConsumer is dropped.
                // This is just for demonstration purposes of API timeout handling.
                let timer_tx = self.timer_tx.get_or_init(|| {
                    let (timer_tx, timer_rx) = mpsc::sync_channel::<oneshot::Sender<()>>(0);
                    thread::spawn(move || {
                        while let Ok(reply_tx) = timer_rx.recv() {
                            thread::sleep(Duration::from_millis(RECEIVE_TIMEOUT_MS));
                            let _ = reply_tx.send(());
                        }
                    });
                    timer_tx
                });
                timer_tx
                    .send(tx)
                    .expect("Timer thread unexpectedly stopped");
                let timeout_future = rx.map(|_| ());
                let (mut sample_buf, result) = self
                    .tire_subscriber
                    .cancellable_receive(sample_buf, 1, 3, timeout_future)
                    .await;
                match result {
                    Ok(0) => Ok("No tire data received".to_string()),
                    Ok(x) => {
                        let sample = sample_buf.pop_front().unwrap();
                        Ok(format!("{x} samples received: sample[0] = {:?}", *sample))
                    }
                    Err(e) => Err(e),
                }
            }
            _ => unreachable!(
                "Streaming mode should use run_receive with stream processing, not read_tire_data"
            ),
        }
    }

    /// Receive `count` tire data samples, printing each result.
    ///
    /// Sync modes add a polling delay between iterations; async/stream modes
    /// block until data arrives.
    pub async fn run_receive(&mut self, mode: &ExampleType, count: usize) {
        if matches!(mode, ExampleType::Streaming) {
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
            return;
        }

        for _ in 0..count {
            match self.read_tire_data(mode).await {
                Ok(data) => println!("{data}"),
                Err(e) => eprintln!("Failed to receive tire data: {:?}", e),
            }
            if matches!(mode, ExampleType::Sync | ExampleType::AsyncServiceDiscovery) {
                thread::sleep(Duration::from_millis(SYNC_RECEIVE_POLL_INTERVAL_MS));
            }
        }
    }

    /// Unsubscribe from all events and release the consumer.
    pub fn unsubscribe(self) {
        let _ = self.tire_subscriber.unsubscribe();
        let _ = self._exhaust_subscriber.unsubscribe();
    }
}
