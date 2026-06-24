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

//! Consumer (proxy) binary for the BigData com-api SCT.
//! it use the async APIs for Service Discovery and Receive samples.
//!
//! # Usage
//!
//! ```
//! bigdata-consumer -n <num_cycles> -m <receive_mode>
//! ```
//! `-n <num_cycles>` — number of time try to receive samples before exiting (required).
//! `-m <receive_mode>` — which async receive mode to test: `with-cancellation`, `without-cancellation`, or `stream` (default: `without-cancellation`).
//! The consumer subscribes to the producer's `map_api_lanes_stamped_` output and
//! prints the `x` field of each received sample until it has received the specified number of samples, at which point it exits.

use bigdata_com_api_gen::{BigDataInterface, MapApiLanesStamped};
use clap::Parser;
use com_api::{
    Builder, FindServiceSpecifier, InstanceSpecifier, LolaRuntimeBuilderImpl, Runtime,
    RuntimeBuilder, SampleContainer, ServiceDiscovery, Subscriber, Subscription,
};
use core::time::Duration;
use core::unreachable;
use futures::channel::oneshot;
use futures::{FutureExt, StreamExt};
use std::path::Path;
use std::sync::mpsc;
use std::thread;

const CONFIG_PATH: &str = "etc/config.json";
const MAX_SAMPLES_PER_CALL: usize = 5;

#[derive(Clone, clap::ValueEnum, Debug)]
enum ReceiveMode {
    WithCancellation,
    WithoutCancellation,
    Stream,
}

#[derive(Parser)]
struct Args {
    /// Number of time to call receive before exiting
    #[arg(short = 'n', required = true)]
    num_cycles: usize,
    /// Receive mode: with-cancellation, without-cancellation, or stream
    #[arg(short = 'm', long, default_value = "without-cancellation")]
    receive_mode: ReceiveMode,
}

/// Demonstrates using the async receive API without cancellation.
/// Receives samples in batches, waiting for at least 1 sample and up to MAX_SAMPLES_PER_CALL samples per call.
async fn receive_without_cancellation<R: Runtime>(
    subscription: impl Subscription<MapApiLanesStamped, R>,
    num_cycles: usize,
) {
    let mut buffer = SampleContainer::new(MAX_SAMPLES_PER_CALL);
    let mut total_cycle = 0;

    println!("[bigdata-consumer] Using async receive without cancellation");

    while total_cycle < num_cycles {
        // Async receive: waits for min_samples (1), receives up to max_samples (MAX_SAMPLES_PER_CALL)
        let (returned_buf, result) = subscription.receive(buffer, 1, MAX_SAMPLES_PER_CALL).await;

        match result {
            // Just for demonstration, we are populating the buffer with received samples and printing them, but in a real application you would likely process them differently.
            Ok(count) => {
                println!("[bigdata-consumer] Received {} samples", count);
                let mut buf = returned_buf;
                while let Some(sample) = buf.pop_front() {
                    println!("[bigdata-consumer]   Sample x: {}", sample.x);
                }
                buffer = buf;
            }
            Err(e) => {
                eprintln!("[bigdata-consumer] Failed to receive samples: {:?}", e);
                buffer = returned_buf;
            }
        }
        total_cycle += 1;
        if total_cycle >= num_cycles {
            break;
        }
    }
}

/// Demonstrates using the async cancellable receive API with a timeout implemented via a oneshot channel.
/// The receive operation will be cancelled if the timeout elapses before samples are received, allowing the consumer to handle timeouts gracefully.
/// Receives samples in batches, waiting for at least 1 sample and up to MAX_SAMPLES_PER_CALL samples per call, with cancellation on timeout.
async fn receive_with_cancellation<R: Runtime>(
    subscription: impl Subscription<MapApiLanesStamped, R>,
    num_cycles: usize,
) {
    let mut buffer = SampleContainer::new(MAX_SAMPLES_PER_CALL);
    let mut total_cycle = 0;

    println!("[bigdata-consumer] Using async cancellable receive with timeout");

    // Thread for simulating a timeout mechanism.
    // This is just for demonstration purposes, but in a real application you would likely use a more robust timer mechanism or library.
    let (timer_tx, timer_rx) = mpsc::channel::<oneshot::Sender<()>>();
    thread::spawn(move || {
        while let Ok(reply_tx) = timer_rx.recv() {
            thread::sleep(Duration::from_millis(50));
            let _ = reply_tx.send(());
        }
    });

    while total_cycle < num_cycles {
        // Request a timeout from the shared timer thread.
        let (tx, rx) = oneshot::channel();
        timer_tx
            .send(tx)
            .expect("Timer thread unexpectedly stopped");

        // Map the receiver to resolve to () instead of Result<(), Canceled>
        let timeout_future = rx.map(|_| ());

        let (returned_buf, result) = subscription
            .cancellable_receive(buffer, 1, MAX_SAMPLES_PER_CALL, timeout_future)
            .await;

        match result {
            // Just for demonstration, we are populating the buffer with received samples and printing them, but in a real application you would likely process them differently.
            Ok(count) => {
                println!("[bigdata-consumer] Received {} samples", count);
                let mut buf = returned_buf;
                while let Some(sample) = buf.pop_front() {
                    println!("[bigdata-consumer]   Sample x: {}", sample.x);
                }
                buffer = buf;
            }
            Err(e) => {
                // Just for demonstration, we are printing the error, but in a real application you would likely handle it differently.
                eprintln!("[bigdata-consumer] Receive error or timeout: {:?}", e);
                buffer = returned_buf;
            }
        }
        total_cycle += 1;
        if total_cycle >= num_cycles {
            break;
        }
    }
}

/// Demonstrates using the async stream API to receive samples as they arrive.
/// Receives samples one at a time as they are published by the producer, printing each sample's `x` field until the specified number of samples have been received.
/// The stream will end if the subscription is cancelled or if the consumer is shut down, but will not end on receive errors instead,
/// errors are returned as part of the stream and can be handled by the consumer while continuing to receive future samples.
async fn receive_stream<R: Runtime>(
    mut subscription: impl Subscription<MapApiLanesStamped, R>,
    num_cycles: usize,
) {
    let mut stream = subscription.to_stream();
    let mut total_cycle = 0;

    println!("[bigdata-consumer] Using async stream");

    while total_cycle < num_cycles {
        match stream.next().await {
            Some(Ok(sample)) => {
                println!("[bigdata-consumer] Stream received sample x: {}", sample.x);
            }
            //when error received from stream, here we are just printing it, but in a real application you would likely handle it differently based on user needs and error type.
            //But library never ends the stream, it will keep retrying to receive samples until the subscription is cancelled or the consumer is shut down.
            Some(Err(e)) => {
                eprintln!("[bigdata-consumer] Stream error: {:?}", e);
            }
            None => unreachable!("Stream never sends None"),
        }
        total_cycle += 1;
    }
}

fn main() {
    futures::executor::block_on(async_main());
}

async fn async_main() {
    let args = Args::parse();
    let num_cycles = args.num_cycles;

    println!(
        "[bigdata-consumer] Starting, will try to receive samples {} times using mode {:?}",
        num_cycles, args.receive_mode
    );

    // Initialise the Lola runtime.
    let mut runtime_builder: LolaRuntimeBuilderImpl = LolaRuntimeBuilderImpl::new();
    runtime_builder.load_config(Path::new(CONFIG_PATH));
    let runtime = runtime_builder
        .build()
        .expect("Failed to build Lola runtime");

    let instance_specifier = InstanceSpecifier::new("/score/cp60/MapApiLanesStamped")
        .expect("Invalid instance specifier");

    let discovery = runtime
        .find_service::<BigDataInterface>(FindServiceSpecifier::Specific(instance_specifier));

    // Await the producer to become available.
    let instances = discovery
        .get_available_instances_async()
        .await
        .expect("Failed to get available instances");

    let builder = instances
        .into_iter()
        .next()
        .expect("No service instances available");
    let consumer = builder.build().expect("Failed to build consumer");

    // Subscribe with a slot buffer large enough for MAX_SAMPLES_PER_CALL.
    let subscription = consumer
        .map_api_lanes_stamped_
        .subscribe(MAX_SAMPLES_PER_CALL)
        .expect("Failed to subscribe to map_api_lanes_stamped_");

    match args.receive_mode {
        ReceiveMode::WithoutCancellation => {
            receive_without_cancellation(subscription, num_cycles).await
        }
        ReceiveMode::WithCancellation => receive_with_cancellation(subscription, num_cycles).await,
        ReceiveMode::Stream => receive_stream(subscription, num_cycles).await,
    }

    println!(
        "[bigdata-consumer] Completed {} receive attempts, exiting",
        num_cycles
    );
}
