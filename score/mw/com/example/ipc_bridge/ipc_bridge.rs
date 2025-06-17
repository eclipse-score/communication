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
use std::path::Path;
use std::thread::sleep;
use std::time::Duration;
use std::pin::pin;

use futures::{Stream, StreamExt};

const SERVICE_DISCOVERY_SLEEP_DURATION: Duration = Duration::from_secs(1);
const DATA_RECEPTION_COUNT: usize = 100;

/// Async function that takes `count` samples from the stream and prints the `x` field of each
/// sample that is received.
async fn get_samples<'a, S: Stream<Item=proxy_bridge_rs::SamplePtr<'a, lib_gen_rs::MapApiLanesStamped>> + 'a>(map_api_lanes_stamped: S, count: usize) {
    let map_api_lanes_stamped = pin!(map_api_lanes_stamped);
    let mut limited_map_api_lanes_stamped = map_api_lanes_stamped.take(count);
    while let Some(data) = limited_map_api_lanes_stamped.next().await {
        println!("Received sample: {}", data.x);
    }
    println!("Stream ended");
}

/// Deliberately add Send to ensure that this is a future that can also be run by multithreaded
/// executors.
fn run<F: std::future::Future<Output=()> + Send>(future: F) {
    futures::executor::block_on(future);
}

fn main() {
    println!("[Rust] Size of MapApiLanesStamped: {}", std::mem::size_of::<lib_gen_rs::MapApiLanesStamped>());
    println!("[Rust] Size of MapApiLanesStamped::lane_boundaries: {}", std::mem::size_of_val(&lib_gen_rs::MapApiLanesStamped::default().lane_boundaries));
    proxy_bridge_rs::initialize(Some(Path::new("./platform/aas/mw/com/example/ipc_bridge/etc/mw_com_config.json")));

    let instance_specifier = proxy_bridge_rs::InstanceSpecifier::try_from("xpad/cp60/MapApiLanesStamped")
        .expect("Instance specifier creation failed");

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

    let lib_gen_rs::IpcBridgeProxy { map_api_lanes_stamped } = lib_gen_rs::IpcBridgeProxy::new(&handles[0])
        .expect("Failed to create the proxy");
    let mut subscribed_map_api_lanes_stamped = map_api_lanes_stamped
        .subscribe(1).expect("Failed to subscribe");
    println!("Subscribed!");
    let map_api_lanes_stamped_stream = subscribed_map_api_lanes_stamped
        .as_stream().expect("Failed to convert to stream");
    run(get_samples(map_api_lanes_stamped_stream, DATA_RECEPTION_COUNT));
}
