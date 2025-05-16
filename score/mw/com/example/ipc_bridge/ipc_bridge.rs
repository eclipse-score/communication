/********************************************************************************
* Copyright (c) 2025 Contributors to the Eclipse Foundation
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
use std::path::PathBuf;
use std::pin::pin;
use std::thread::sleep;
use std::time::Duration;

use clap::{Parser, ValueEnum};
use futures::{Stream, StreamExt};

#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd, Ord, ValueEnum)]
enum Mode {
    /// Act as a data sender
    Send,
    /// Equivalent to send
    Skeleton,
    /// Act as a data receiver
    Recv,
    /// Equivalent to recv
    Proxy,
}

#[derive(Parser)]
struct Arguments {
    /// Set to either send/skeleton or recv/proxy to determine the role of the process
    #[arg(value_enum, short, long)]
    mode: Mode,
    #[arg(
        short,
        long,
        default_value = "./platform/aas/mw/com/example/ipc_bridge/etc/mw_com_config.json"
    )]
    service_instance_manifest: PathBuf,
}

const SERVICE_DISCOVERY_SLEEP_DURATION: Duration = Duration::from_secs(1);
const DATA_RECEPTION_COUNT: usize = 100;

/// Async function that takes `count` samples from the stream and prints the `x` field of each
/// sample that is received.
async fn get_samples<
    'a,
    S: Stream<Item = proxy_bridge_rs::SamplePtr<'a, lib_gen_rs::MapApiLanesStamped>> + 'a,
>(
    map_api_lanes_stamped: S,
    count: usize,
) {
    let map_api_lanes_stamped = pin!(map_api_lanes_stamped);
    let mut limited_map_api_lanes_stamped = map_api_lanes_stamped.take(count);
    while let Some(data) = limited_map_api_lanes_stamped.next().await {
        println!("Received sample: {}", data.x);
    }
    println!("Stream ended");
}

/// Deliberately add Send to ensure that this is a future that can also be run by multithreaded
/// executors.
fn run<F: std::future::Future<Output = ()> + Send>(future: F) {
    futures::executor::block_on(future);
}

fn run_recv_mode(instance_specifier: proxy_bridge_rs::InstanceSpecifier) {
    let handles = loop {
        let handles = proxy_bridge_rs::find_service(instance_specifier.clone())
            .expect("Instance specifier resolution failed");
        if handles.len() > 0 {
            break handles;
        } else {
            println!("No service found, retrying in 1 second");
            sleep(SERVICE_DISCOVERY_SLEEP_DURATION);
        }
    };

    let lib_gen_rs::IpcBridge::Proxy {
        map_api_lanes_stamped_,
    } = lib_gen_rs::IpcBridge::Proxy::new(&handles[0]).expect("Failed to create the proxy");
    let mut subscribed_map_api_lanes_stamped = map_api_lanes_stamped_
        .subscribe(1)
        .expect("Failed to subscribe");
    println!("Subscribed!");
    let map_api_lanes_stamped_stream = subscribed_map_api_lanes_stamped
        .as_stream()
        .expect("Failed to convert to stream");
    run(get_samples(
        map_api_lanes_stamped_stream,
        DATA_RECEPTION_COUNT,
    ));
}

fn run_send_mode(instance_specifier: proxy_bridge_rs::InstanceSpecifier) {
    let skeleton = lib_gen_rs::IpcBridge::Skeleton::new(&instance_specifier)
        .expect("BigDataSkeleton creation failed");

    let skeleton = skeleton.offer_service().expect("Failed offering from rust");
    let mut x: u32 = 1;
    while x < 10 {
        let mut sample: lib_gen_rs::MapApiLanesStamped = lib_gen_rs::MapApiLanesStamped::default();
        sample.x = x;
        skeleton
            .events
            .map_api_lanes_stamped_
            .send(sample)
            .expect("Failed sending event");

        println!("published {} sleeping", x);
        x += 1;
        sleep(Duration::from_millis(100));
    }

    println!("stopping offering and sleeping for 5sec");
    sleep(Duration::from_secs(5));
    let skeleton = skeleton.stop_offer_service();

    let skeleton = skeleton.offer_service().expect("Reoffering failed");
    x = 0;
    while x < 10 {
        let mut sample: lib_gen_rs::MapApiLanesStamped = lib_gen_rs::MapApiLanesStamped::default();
        sample.x = x;
        skeleton
            .events
            .map_api_lanes_stamped_
            .send(sample)
            .expect("Failed sending event");

        println!("published {} sleeping", x);
        x += 1;
        sleep(Duration::from_millis(100));
    }
}

fn main() {
    let args = Arguments::parse();
    println!(
        "[Rust] Size of MapApiLanesStamped: {}",
        std::mem::size_of::<lib_gen_rs::MapApiLanesStamped>()
    );
    println!(
        "[Rust] Size of MapApiLanesStamped::lane_boundaries: {}",
        std::mem::size_of_val(&lib_gen_rs::MapApiLanesStamped::default().lane_boundaries)
    );
    proxy_bridge_rs::initialize(Some(&args.service_instance_manifest));

    let instance_specifier =
        proxy_bridge_rs::InstanceSpecifier::try_from("xpad/cp60/MapApiLanesStamped")
            .expect("Instance specifier creation failed");

    match args.mode {
        Mode::Send | Mode::Skeleton => {
            println!("Running in Send/Skeleton mode");
            run_send_mode(instance_specifier);
        }
        Mode::Recv | Mode::Proxy => {
            println!("Running in Recv/Proxy mode");
            run_recv_mode(instance_specifier);
        }
    }
}
